"""Flask routes for the single-user, loopback-only web application."""
from __future__ import annotations
from ssh_transport import connection_options


from dataclasses import asdict
from collections import Counter
import base64
import csv
import json
import os
from pathlib import Path
import queue
import re
import secrets
import shutil
import subprocess
import sys
import threading
import time
import tarfile
from datetime import datetime

from flask import Flask, Response, abort, jsonify, redirect, render_template, request, session, send_file, url_for
from werkzeug.exceptions import HTTPException
from slurmy_archives import is_archive

from .monitor import RemoteCollector, summarize_results, ResultRecord
from .cluster import fetch_status
from .launcher import web_version
from . import workflows

REPO = workflows.REPO
STATE = REPO / 'web-app/.state'


class BuilderCancelled(Exception):
    """A local workflow preparation was cancelled before saving began."""


def safe_job(value):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', value):
        raise ValueError('Invalid job ID.')
    return value


def display_name(value):
    if not isinstance(value, str):
        raise ValueError('Job name must be text.')
    value = value.strip()
    if not value or len(value) > 100 or any(ord(char) < 32 or ord(char) == 127 for char in value):
        raise ValueError('Job name must be 1–100 characters with no control characters.')
    return value


def renamed_job_id(old_id, name):
    prefix = re.sub(r'[^A-Za-z0-9_.-]', '-', display_name(name)).lstrip('._-')[:48] or 'job'
    suffix = re.search(r'(_[0-9]{9,}(?:_[0-9]+)?)$', old_id)
    return safe_job(prefix + (suffix.group(1) if suffix else '_' + old_id))


def summary(job):
    workflow_directory = current_workflow_directory(job.metadata.get('workflow_directory', ''))
    workflow_name = Path(workflow_directory).name if workflow_directory else ''
    if workflow_name.startswith('example-') and Path(workflow_directory).parent == workflows.USER_RUNS:
        workflow_name = workflow_name.removeprefix('example-')
    name = job.metadata.get('job_name') or workflow_name or job.job_name
    return dict(id=job.job_id, name=name, state=job.state,
                total=job.task_count, completed=job.effective_completed,
                percent=job.percent, issues=job.issue_count, created=job.created_epoch,
                directory=workflow_directory, workflow_name=workflow_name,
                outcomes=history_outcomes(job),
                submission_state=job.submission_state, submitted_batches=job.submission_count,
                batch_count=job.batch_count,
                statuses=job.status_counts)


def current_workflow_directory(value):
    """Keep jobs submitted before the repository move linked to their workflow."""
    if not value:
        return ''
    old_root = str(REPO)
    for old, new in (('ExampleRuns', 'workflows/example-workflows'),
                     ('YourRuns', 'workflows/my-workflows')):
        prefix = old_root + '/' + old + '/'
        if str(value).startswith(prefix):
            return old_root + '/' + new + '/' + str(value)[len(prefix):]
    return str(value)


def call_outcome(result):
    """Return the same display label and source used by the pie chart and call filter."""
    fallback = {'ok': 'OK', 'time-limit': 'Time limit',
                'counter-satisfiable': 'CounterSatisfiable',
                'memory-limit': 'Memory limit'}
    if result is None:
        return 'Not finished', ''
    szs = result.szs_status.strip()
    if result.status == 'solver-error' and szs.casefold() in {
            'error', 'oserror', 'inputerror', 'syntaxerror', 'semanticerror',
            'typeerror', 'usageerror'}:
        return 'Solver error', 'execution'
    if szs:
        return szs, 'solver'
    label = fallback.get(result.status, result.status.replace('-', ' ').capitalize())
    source = ('resource limiter' if result.status in
              {'time-limit', 'memory-limit', 'resource-limiter-error'} else
              'controller' if result.status in {'worker-error', 'interrupted'} else
              'execution')
    return label, source


def history_outcomes(job):
    """Use compact final-result counts; live progress has no published answer yet."""
    counts = Counter()
    outcomes = getattr(job, 'outcome_counts', {})
    if outcomes:
        for (status, szs), count in outcomes.items():
            if count > 0:
                counts[call_outcome(ResultRecord(0, True, status, szs_status=szs))] += count
    elif job.results:
        for result in job.results.values():
            if result.complete:
                counts[call_outcome(result)] += 1
    else:
        for status, count in job.status_counts.items():
            counts[call_outcome(ResultRecord(0, True, status))] += count
    published = sum(counts.values())
    live_finished = max(0, job.effective_completed - published)
    if live_finished:
        counts[('Finished · outcome pending', 'controller')] += live_finished
    unfinished = max(0, job.task_count - sum(counts.values()))
    if unfinished:
        counts[('Not finished', '')] += unfinished
    return [dict(label=label, source=source, count=count) for (label, source), count in
            sorted(counts.items(), key=lambda item: (item[0][0] != 'Not finished', item[0])) if count > 0]


def outcome_breakdown(job):
    """Count final SZS answers or fallback execution outcomes across all calls."""
    counts = Counter()
    by_solver = Counter()
    solver_names = {definition.solver or 'Unknown solver' for definition in job.tasks.values()}
    for task_id, result in job.results.items():
        label, source = call_outcome(result)
        counts[(label, source)] += 1
        solver = job.tasks.get(task_id)
        by_solver[(label, source, solver.solver if solver and solver.solver else 'Unknown solver')] += 1
    total = max(job.task_count, len(job.tasks), len(job.results))
    unfinished = max(0, total - len(job.results))
    if unfinished:
        counts[('Not finished', '')] += unfinished
        for task_id, definition in job.tasks.items():
            if task_id not in job.results:
                by_solver[('Not finished', '', definition.solver or 'Unknown solver')] += 1
        untracked = unfinished - sum(count for (label, source, _), count in by_solver.items() if label == 'Not finished')
        if untracked > 0:
            by_solver[('Not finished', '', 'Unknown solver')] += untracked
    priority = {('theorem', 'solver'): 0,
                ('contradictoryaxioms', 'solver'): 1,
                ('countersatisfiable', 'solver'): 2,
                ('unsatisfiable', 'solver'): 3,
                ('satisfiable', 'solver'): 4,
                ('timeout', 'solver'): 5,
                ('solver error', 'execution'): 6,
                ('time limit', 'resource limiter'): 7,
                ('memory limit', 'resource limiter'): 8,
                ('resource limiter error', 'resource limiter'): 9,
                ('not finished', ''): 11}
    return [{'label': label, 'source': source, 'count': count,
             'solvers': [{'name': name, 'count': by_solver[(label, source, name)]}
                         for name in sorted(solver_names | {name for (outcome, origin, name) in by_solver
                                                           if (outcome, origin) == (label, source)}, key=str.casefold)]}
            for (label, source), count in sorted(
                counts.items(), key=lambda item: (priority.get((item[0][0].casefold(), item[0][1]), 10),
                                                  item[0][0].casefold(), item[0][1]))]


def submission_phase(log):
    """Summarize the newest visible step of a web-triggered submission."""
    phase = 'Preparing submission files…'
    for line in log.splitlines():
        if line.startswith(('Preparation progress: ', 'Build progress: ', 'Packaging progress: ', 'Submission phase: ', 'Submission waiting: ',
                            'Submission progress: ', 'Submission complete: ')):
            phase = line
        elif match := re.fullmatch(r'Batch \d+: SLURM \d+ \((\d+/\d+) accepted\)', line):
            phase = f'Submitting batches: {match.group(1)} accepted'
    return phase


def unfinished_call(job, task_id, progress, batch_records=None):
    """Explain a missing result using this solver-problem pair's actual SLURM batch."""
    if job.submission_state == 'stopped':
        return 'stopped', 'After resume', 'This job was stopped. Resume it to run this unfinished call.', ''
    batch_id = job.task_batches.get(task_id)
    if batch_id is None and progress:
        batch_id = progress.batch_id
    if batch_id is None:
        batch_id = task_id // job.batch_size
    if batch_records is None:
        records = [record for record in job.slurm
                   if record.array_task_id.isdigit()
                   and job.submission_offsets.get(record.array_job_id, 0) + int(record.array_task_id) == batch_id]
        records.sort(key=lambda record: record.source != 'queue')
        record = records[0] if records else None
    else:
        record=batch_records.get(batch_id)
    log = ''
    if record:
        log = job.logs.get(f'slurm_{record.array_job_id}_{record.array_task_id}.out', '')
    excerpt = next((line.strip() for line in reversed(log.splitlines()) if line.strip()), '')[:240]
    suffix = f' Last SLURM log line: {excerpt}' if excerpt else ''
    if record and record.source == 'queue':
        code = record.state.upper().split()[0]
        if code in {'RUNNING', 'COMPLETING'}:
            if progress and progress.state == 'running':
                return 'running', 'Running', f'Solver-problem pair started in SLURM batch {batch_id}; no result saved yet.', log
            return 'batch starting', 'Yes', f'SLURM batch {batch_id} is {code.lower()}, but this solver-problem pair has not recorded a start yet.{suffix}', log
        return 'queued', 'Yes', f'SLURM batch {batch_id} is {code.lower()}: {record.location}.{suffix}', log
    if record:
        code = record.state.upper().split()[0].rstrip('+')
        label = {'COMPLETED': 'result missing', 'TIMEOUT': 'batch timed out',
                 'OUT_OF_MEMORY': 'batch out of memory', 'NODE_FAIL': 'node failed',
                 'CANCELLED': 'batch cancelled', 'FAILED': 'batch failed',
                 'PREEMPTED': 'batch preempted'}.get(code, 'batch ' + code.lower())
        reason = (f'SLURM batch {batch_id} ended as {code}, but this solver-problem pair has no saved result.'
                  f'{suffix} See its batch log for details.')
        return label, 'No', reason, log
    if progress and progress.state != 'running':
        return 'result missing', 'No', f'Solver-problem pair recorded {progress.state}, but no result was saved. SLURM no longer reports batch {batch_id}.', log
    if batch_id not in job.submitted_batches and job.submitted_batches:
        if job.submission_state == 'submitting':
            return 'awaiting submission', 'Yes', f'Batch {batch_id} is waiting for a free SLURM submission slot.', log
        return 'not submitted', 'No', f'Batch {batch_id} was planned but has no SLURM submission. Submission may have stopped partway through.', log
    if not job.submission_offsets and not job.slurm:
        recent = time.time() - job.created_epoch < 180
        return ('preparing submission', 'Unknown', 'No SLURM submission recorded yet; preparation may still be underway.', log) if recent else (
            'not submitted', 'No', 'No SLURM submission was recorded for this job.', log)
    return 'scheduler status unknown', 'Unknown', f'No current or historical SLURM record is available for batch {batch_id}; cannot tell whether it will run.', log


def batch_allocations(job):
    cached=getattr(job,"_batch_allocations",None)
    if cached is not None:return cached
    batch_records={}
    for record in sorted(job.slurm,key=lambda item:item.source!='queue'):
        if record.array_task_id.isdigit():
            batch=job.submission_offsets.get(record.array_job_id,0)+int(record.array_task_id)
            batch_records.setdefault(batch,record)
    job._batch_allocations=batch_records
    return batch_records


def task_rows(job,task_ids=None):
    batch_records=batch_allocations(job)
    for task_id in (sorted(set(job.tasks) | set(job.results)) if task_ids is None else task_ids):
        definition = job.tasks.get(task_id)
        result = job.results.get(task_id)
        progress = job.call_progress.get(task_id)
        state = 'pending'
        will_run = ''
        reason = ''
        diagnostic = ''
        wall = None
        batch_id = job.task_batches.get(task_id, task_id // job.batch_size)
        allocation=batch_records.get(batch_id)
        if result:
            state, wall = result.szs_status or result.status.replace('-', ' '), result.wall_seconds
            if result.status in {'resource-limiter-error', 'worker-error', 'interrupted'}:
                state = result.status.replace('-', ' ')
            elif result.status == 'error' and not result.szs_status:
                state = 'execution error'
        else:
            if not progress:
                # Historical sequential-batch jobs remain inspectable.
                legacy = job.progress.get(task_id // job.batch_size)
                if legacy and legacy.task_id == task_id:
                    progress = legacy
            state, will_run, reason, diagnostic = unfinished_call(job, task_id, progress,batch_records)
            if progress and progress.state == 'running' and state == 'running':
                wall = max(0, time.time() - progress.started_epoch) if progress.started_epoch else None
        yield dict(id=task_id, solver=definition.solver if definition else '',
                   problem=definition.problem if definition else '',
                   command=definition.command if definition else '',
                   limiter_command=definition.limiter_command if definition else '',
                   directory=definition.solver_root if definition else '',
                   state=state, wall=wall, cpu=result.cpu_seconds if result else None,
                   memory=result.max_memory_kib * 1024 if result and result.max_memory_kib is not None else None,
                   return_code=result.return_code if result else '',
                   execution_status=result.status if result else '',
                   szs_status=result.szs_status if result else '',
                   szs_output=result.szs_output if result else '',
                   will_run=will_run, reason=reason, diagnostic=diagnostic,
                   node=(allocation.location if allocation and allocation.state.upper().startswith(('RUNNING', 'COMPLETING')) else ''),
                   slurm_id=allocation.display_id if allocation else '',
                   output=bool(result and result.archive))


CALL_SORT_FIELDS = {'id', 'solver', 'problem', 'state', 'will_run', 'node', 'reason', 'cpu', 'wall', 'memory'}


def sorted_call_rows(rows, field, direction):
    if field not in CALL_SORT_FIELDS or direction not in {'asc', 'desc'}:
        abort(400, 'Invalid solver-problem pair sort order.')
    # Keep absent measurements last in either direction, and IDs stable for ties.
    present = [row for row in rows if row[field] is not None and row[field] != '']
    missing = [row for row in rows if row[field] is None or row[field] == '']
    present.sort(key=lambda row: (row[field].casefold() if isinstance(row[field], str) else row[field], row['id']),
                 reverse=direction == 'desc')
    return present + missing


def task_page(job,page,per_page,field,direction,query='',outcomes=None,task_ids=None):
    """Only build rich display rows for the page unless search/sort requires them."""
    if field not in CALL_SORT_FIELDS or direction not in {'asc','desc'}:
        abort(400,'Invalid solver-problem pair sort order.')
    if task_ids is None:
        if not hasattr(job,'_view_ids'):job._view_ids=sorted(set(job.tasks)|set(job.results))
        ids=job._view_ids
    else:ids=sorted(task_ids)
    if outcomes:ids=[task_id for task_id in ids if call_outcome(job.results.get(task_id)) in outcomes]
    start=page*per_page;end=start+per_page
    if query or field in {'state','will_run','node','reason'}:
        # These fields include derived scheduler diagnostics. Searching all
        # displayed fields or globally sorting diagnostics genuinely needs them.
        rows=[row for row in task_rows(job,ids) if not query or query in ' '.join(str(v) for v in row.values()).lower()]
        return sorted_call_rows(rows,field,direction)[start:end],len(rows)
    if field=='id':
        ordered=ids if direction=='asc' else list(reversed(ids))
    else:
        def value(task_id):
            definition=job.tasks.get(task_id);result=job.results.get(task_id)
            if field in {'solver','problem'}:return getattr(definition,field,'')
            if field=='cpu':return result.cpu_seconds if result else None
            if field=='memory':return result.max_memory_kib*1024 if result and result.max_memory_kib is not None else None
            if result:return result.wall_seconds
            progress=job.call_progress.get(task_id) or job.progress.get(task_id//job.batch_size)
            if progress and progress.task_id==task_id and progress.state=='running':
                return next(task_rows(job,[task_id]))['wall']
            return None
        keys={task_id:value(task_id) for task_id in ids}
        present=[task_id for task_id in ids if keys[task_id] is not None and keys[task_id]!='']
        missing=[task_id for task_id in ids if keys[task_id] is None or keys[task_id]=='']
        present.sort(key=lambda task_id:(keys[task_id].casefold() if isinstance(keys[task_id],str) else keys[task_id],task_id),reverse=direction=='desc')
        ordered=present+missing
    return list(task_rows(job,ordered[start:end])),len(ids)


def call_page_args(default_sort='id'):
    try:
        page = int(request.args.get('page', '0'))
        per_page = int(request.args.get('per_page', '100'))
    except ValueError:
        abort(400, 'Page and page size must be numbers.')
    if page < 0 or not 1 <= per_page <= 10000:
        abort(400, 'Page must be nonnegative and page size must be 1–10000.')
    return page, per_page, request.args.get('sort', default_sort), request.args.get('direction', 'asc')


def create_app():
    workflows.migrate_user_runs()
    app = Flask(__name__)
    startup_web_version = web_version()
    app.config.update(SECRET_KEY=secrets.token_hex(32), MAX_CONTENT_LENGTH=2 * 1024 * 1024,
                      SESSION_COOKIE_HTTPONLY=True, SESSION_COOKIE_SAMESITE='Strict',
                      TRUSTED_HOSTS=['127.0.0.1', 'localhost'])
    cache, locks, operations, announced_jobs = {}, {}, {}, {}
    cluster_cache = {}
    problem_text_cache = {}
    guard = threading.Lock()
    builder_lock = threading.Lock()
    builder_operations = {}

    @app.before_request
    def local_only():
        if request.path == '/api/workflows/import':
            request.max_content_length = 10 * 1024**3
        if request.remote_addr not in {'127.0.0.1', '::1', None}:
            abort(403)
        if request.headers.get('Sec-Fetch-Site') == 'cross-site' and (
                request.method != 'GET' or request.path.startswith('/api/')):
            abort(403)
        origin = request.headers.get('Origin')
        if origin and origin != request.host_url.rstrip('/'):
            abort(403)
        if request.method == 'POST':
            token = session.get('csrf')
            if not token or not secrets.compare_digest(token, request.headers.get('X-CSRF-Token', '')):
                abort(403, 'Reload the page before making changes.')

    @app.after_request
    def headers(response):
        response.headers['Content-Security-Policy'] = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"
        response.headers['X-Content-Type-Options'] = 'nosniff'
        response.headers['Referrer-Policy'] = 'same-origin'
        response.headers['Cache-Control'] = 'no-store'
        return response

    @app.errorhandler(Exception)
    def error(exc):
        if isinstance(exc, HTTPException):
            code, message = exc.code, exc.description
        elif isinstance(exc, (ValueError, OSError, RuntimeError, KeyError, TypeError)):
            code, message = 400, str(exc)
        else:
            app.logger.exception('Request failed')
            code, message = 500, 'Unexpected error. See web-app/.state/server.log.'
        if request.path.startswith('/api/'):
            return jsonify(error=message), code
        selected_host = request.args.get('host') or os.environ.get('SLURMY_HOST', 'datalab')
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', selected_host):
            selected_host = 'datalab'
        session.setdefault('csrf', secrets.token_hex(32))
        return render_template('error.html', message=message, host=selected_host, csrf=session['csrf']), code

    def host():
        return workflows.alias(request.args.get('host') or os.environ.get('SLURMY_HOST', 'datalab'))

    def snapshot(detail=None, force=False):
        key = (host(), detail)
        with guard:
            lock = locks.setdefault(key, threading.Lock())
        with lock:
            cached = cache.get(key)
            previous = cached[1].find_job(detail) if cached and detail else None
            cache_seconds = 30 if previous and previous.task_count > 10000 else 5
            if cached and not force and time.monotonic() - cached[0] < cache_seconds:
                return cached[1]
            since = (max(0, cached[1].fetched_epoch - 60)
                     if previous and previous.task_count > 10000 and previous.tasks else None)
            value = app.config.get('COLLECTOR', RemoteCollector)(key[0], 10).fetch(detail, since)
            if since is not None:
                current = value.find_job(detail)
                if current:
                    current.tasks = previous.tasks
                    current.results = {**previous.results, **current.results}
                    summarize_results(current, current.results)
            cache[key] = (time.monotonic(), value)
            if len(cache) > 32:
                cache.pop(next(iter(cache)))
            return value

    def job_snapshot(job_id, force=False):
        job = snapshot(safe_job(job_id), force=force).find_job(job_id)
        if not job:
            abort(404, 'Job not found on this SSH host.')
        return job

    def directory():
        requested = current_workflow_directory(request.args.get('directory'))
        if requested:
            old = workflows.path_at(requested, workflows.BASE)
            if old.parent == workflows.LEGACY_RUNS and (workflows.USER_RUNS / old.name).is_dir():
                requested = str(workflows.USER_RUNS / old.name)
        path = workflows.existing(requested, 'directory').resolve()
        allowed = [REPO / 'workflows/example-workflows', workflows.USER_RUNS]
        if not any(path.is_relative_to(root.resolve()) and path != root.resolve() for root in allowed):
            raise ValueError('Choose a workflow under workflows/example-workflows or workflows/my-workflows.')
        if not (path / 'Makefile').is_file():
            raise ValueError('Choose a directory containing a workflow Makefile.')
        return path

    def page(template, **kwargs):
        session.setdefault('csrf', secrets.token_hex(32))
        return render_template(template, host=host(), csrf=session['csrf'], **kwargs)

    @app.get('/health')
    def health():
        return jsonify(app='slurmy-web', repository=str(REPO), pid=os.getpid(),
                       web_version=startup_web_version)

    @app.get('/')
    def index():
        return page('jobs.html')

    @app.get('/cluster')
    def cluster_page():
        return page('cluster.html')

    @app.get('/api/cluster')
    def cluster_status():
        partition = request.args.get('partition', 'CPU-amd')
        key = (host(), partition)
        with guard:
            cached = cluster_cache.get(key)
            if cached and request.args.get('fresh') != '1' and time.monotonic() - cached[0] < 10:
                return jsonify(cached[1])
        value = app.config.get('CLUSTER_COLLECTOR', fetch_status)(*key)
        with guard:
            cluster_cache[key] = (time.monotonic(), value)
            if len(cluster_cache) > 16:
                cluster_cache.pop(next(iter(cluster_cache)))
        return jsonify(value)

    @app.get('/api/cluster/node')
    def cluster_node_jobs():
        partition = request.args.get('partition', 'CPU-amd')
        node = request.args.get('node', '')
        value = app.config.get('CLUSTER_NODE_COLLECTOR', fetch_status)(
            host(), partition, node=node)
        return jsonify(node=node, partition=partition, updated=value['updated'],
                       jobs=[job for job in value['jobs'] if job['state'].upper().startswith(('RUNNING', 'COMPLETING'))])

    @app.get('/jobs/<job_id>')
    def job_page(job_id):
        return page('job.html', job_id=safe_job(job_id))

    @app.get('/jobs/<job_id>/problems/<int:task_id>')
    def problem_page(job_id, task_id):
        return page('problem.html', job_id=safe_job(job_id), task_id=task_id)

    @app.get('/workflows')
    def workflow_list():
        def created_label(path):
            try:
                value = json.loads((path / '.slurmy-workflow.json').read_text()).get('_created_at')
                epoch = float(value) if value is not None else path.stat().st_ctime
            except (OSError, ValueError, TypeError, json.JSONDecodeError):
                epoch = path.stat().st_ctime
            return datetime.fromtimestamp(epoch).astimezone().strftime('%Y-%m-%d %H:%M')
        def entries(root):
            if not root.exists():
                return []
            return [dict(name=(path.name.removeprefix('example-') if root == workflows.USER_RUNS else path.name),
                         directory=str(path.resolve()), created=created_label(path)) for path in sorted(root.iterdir())
                    if path.is_dir() and (path / 'Makefile').is_file()]
        return page('experiments.html', user_workflows=entries(workflows.USER_RUNS),
                    example_workflows=entries(REPO / 'workflows/example-workflows'))

    @app.get('/experiments')
    def legacy_experiments():
        return redirect(url_for('workflow_list', host=host()))

    @app.get('/workflow')
    def workflow():
        path = directory()
        files = {name: (path / name).read_text() for name in
                 ('jobpairs.csv', 'configurations.csv', 'problem-globs.txt', 'axiom-globs.txt', 'building.txt', 'resource_limiter_template.txt', 'Makefile') if (path / name).is_file()}
        name = path.name.removeprefix('example-') if path.parent == workflows.USER_RUNS else path.name
        created = datetime.fromtimestamp(path.stat().st_ctime).astimezone().strftime('%Y-%m-%d %H:%M')
        return page('experiment.html', directory=str(path), name=name, created=created, files=files,
                    base=str(workflows.USER_RUNS.resolve()) + os.sep,
                    prepared=(path / 'submit.sh').is_file())

    @app.get('/experiment')
    def legacy_experiment():
        return redirect(url_for('workflow', host=host(), directory=request.args.get('directory', '')))

    @app.get('/new')
    def new():
        return page('new.html', base=str(workflows.BASE), repo=str(REPO), initial=None, editing=False)

    @app.get('/api/solver-configurations')
    def solver_configurations():
        if 'directory' in request.args:
            path = directory()
            return jsonify(configurations=workflows.copyable_solver_configurations(path))
        roots = (('Example workflows', REPO / 'workflows/example-workflows'),
                 ('My workflows', workflows.USER_RUNS))
        items = [dict(group=group, name=path.name, directory=str(path.resolve()))
                 for group, root in roots if root.is_dir()
                 for path in sorted(root.iterdir()) if path.is_dir() and (path / 'Makefile').is_file()]
        return jsonify(workflows=items)

    @app.get('/api/limiter-configuration')
    def limiter_configuration():
        return jsonify(workflows.copyable_limiter_configuration(directory()))

    @app.get('/workflow/edit')
    def edit_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Duplicate an example before editing it.')
        data = workflows.decisions(path)
        if data.get('limiter_mode') == 'existing':
            data['limiter'] = workflows.copyable_limiter_configuration(path)['invocation']
            data['limiter_mode'] = 'inline'
            data['limiter_file'] = ''
        return page('new.html', base=str(workflows.BASE), repo=str(REPO), initial=data,
                    editing=True, directory=str(path))

    @app.get('/logo.svg')
    def logo():
        return send_file(REPO / 'implementation/logo.svg')

    @app.get('/api/jobs')
    def jobs():
        # Update submission records before copying them: preparation may have
        # replaced metadata, and remote submission may already have printed its ID.
        with guard:
            dispatching = [op for op in operations.values()
                           if op.get('provisional_job_id') and not op.get('job_id') and op['state'] == 'running']
        for operation in dispatching:
            log_path = STATE / (operation['id'] + '.log')
            log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.is_file() else ''
            if 'Submission phase:' in log or re.search(r'^bash ./submit.sh$', log, re.MULTILINE):
                operation['preparation_complete'] = True
            match = re.search(r'^Slurmy job ID: ([A-Za-z0-9._-]+)$', log, re.MULTILINE)
            if match and not operation.get('job_id'):
                announce_operation_job(operation, match.group(1), Path(operation['operation_cwd']),
                                       {'SLURMY_HOST': operation['operation_host']})
            with guard:
                item = announced_jobs.get((operation['operation_host'], operation['provisional_job_id']))
                if item:
                    details = dispatch_metadata(operation, Path(operation['operation_cwd']))
                    item['total'] = int(details.get('task_count', 0) or 0)
                    item['phase'] = submission_phase(log[-16384:])
                    item['state'] = 'DISPATCHING' if operation['state'] == 'running' else operation['state'].upper()
        with guard:
            local = [item.copy() for (item_host, _), item in announced_jobs.items() if item_host == host()]
        value = snapshot()
        items = [summary(job) for job in value.jobs]
        updated = value.fetched_epoch
        remote_ids = {item['id'] for item in items}
        items.extend(item for item in local if item['id'] not in remote_ids)
        folder = request.args.get('directory')
        if folder:
            linked = {folder}
            manifest = Path(folder) / '.slurmy-workflow.json'
            if manifest.is_file():
                linked.update(json.loads(manifest.read_text(encoding='utf-8')).get('_previous_directories', []))
            items = [job for job in items if job['directory'] in linked]
        return jsonify(jobs=items, updated=updated)

    @app.get('/api/jobs/<job_id>')
    def job_data(job_id):
        job = job_snapshot(job_id, force=request.args.get('fresh') == '1')
        query = request.args.get('q', '').lower()
        outcomes = request.args.getlist('outcome')
        origins = request.args.getlist('origin')
        if outcomes and not origins:
            origins = [''] * len(outcomes)
        if len(outcomes) != len(origins) or any(
            not label or len(label) > 100 or len(source) > 40
            for label, source in zip(outcomes, origins)
        ):
            abort(400, 'Invalid outcome filter.')
        selected_outcomes = set(zip(outcomes, origins))
        page_number,per_page,sort,direction=call_page_args()
        rows,matched=task_page(job,page_number,per_page,sort,direction,query,selected_outcomes)
        if not hasattr(job,'_view_outcomes'):job._view_outcomes=outcome_breakdown(job)
        return jsonify(**{**summary(job),"outcomes":job._view_outcomes},tasks=rows,matched=matched)

    @app.post('/api/jobs/<job_id>/name')
    def rename_job(job_id):
        job_id = safe_job(job_id)
        name = display_name((request.get_json() or {}).get('name', ''))
        job = job_snapshot(job_id)
        if job.active_records or job.submission_state == 'submitting':
            abort(409, 'Wait until every SLURM allocation for this job has finished before renaming its directory.')
        with guard:
            if any(op['state'] == 'running' and
                   (op['scope'] == host() + ':' + job_id or op.get('job_id') == job_id)
                   for op in operations.values()):
                abort(409, 'Wait for the active job operation to finish before renaming it.')
        new_id = renamed_job_id(job_id, name)
        local_old = REPO / 'job-results' / job_id
        local_new = REPO / 'job-results' / new_id
        verbose_old = REPO / 'job-results' / 'verbose' / job_id
        verbose_new = REPO / 'job-results' / 'verbose' / new_id
        if local_old.is_symlink() or verbose_old.is_symlink() or (new_id != job_id and
                (local_new.exists() or local_new.is_symlink() or verbose_new.exists() or verbose_new.is_symlink())):
            abort(409, 'The local result directory is a symlink or the new job ID already exists locally.')
        script = (REPO / 'implementation/templates/remote_rename_job.sh').read_text(encoding='utf-8')
        encoded = base64.b64encode(name.encode('utf-8')).decode('ascii')
        try:
            result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--', host(),
                                     'bash', '-s', '--', job_id, new_id, encoded],
                                    input=script, text=True, capture_output=True, timeout=90, check=False)
        except (OSError, subprocess.TimeoutExpired) as exc:
            abort(503, f'Could not confirm the rename over SSH: {exc}. Refresh jobs before retrying.')
        if result.returncode:
            abort(409, result.stderr.strip() or 'Could not rename the cluster job directory.')
        warning = ''
        try:
            if new_id != job_id:
                for old, new in ((local_old, local_new), (verbose_old, verbose_new)):
                    if old.is_dir():
                        old.rename(new)
            for local in (local_new, verbose_new) if new_id != job_id else (local_old, verbose_old):
                if not local.is_dir():
                    continue
                for filename in ('metadata.json', 'diagnostics/metadata.json', 'sync-metadata.json'):
                    path = local / filename
                    if not path.is_file():
                        continue
                    data = json.loads(path.read_text(encoding='utf-8'))
                    if path.name == 'metadata.json':
                        history = data.get('previous_job_ids', [])
                        if job_id != new_id and job_id not in history:
                            history.append(job_id)
                        data.update(job_id=new_id, job_name=name, previous_job_ids=history)
                    else:
                        data.update(slurmy_job_id=new_id, remote_directory=f'$HOME/slurmy/jobs/{new_id}',
                                    local_directory=str(local.resolve()))
                    temporary = path.with_suffix(path.suffix + '.tmp')
                    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
                    temporary.replace(path)
        except (OSError, ValueError, TypeError) as exc:
            warning = f'Cluster job renamed, but local downloaded results need attention: {exc}'
        with guard:
            announced = announced_jobs.pop((host(), job_id), None)
            if announced:
                announced.update(id=new_id, name=name)
                announced_jobs[(host(), new_id)] = announced
            cache.clear()
        return jsonify(id=new_id, name=name, warning=warning)

    @app.get('/api/jobs/<job_id>/output/<int:task_id>')
    def output(job_id, task_id):
        job = job_snapshot(job_id)
        result = job.results.get(task_id)
        if not result or not result.archive:
            abort(404, 'Output is available after this solver-problem pair saves its result archive.')
        value = app.config.get('COLLECTOR', RemoteCollector)(host(), 10).fetch_task_output(job_id, result)
        payload = asdict(value)
        command = job.tasks.get(task_id).limiter_command if task_id in job.tasks else ''
        expected = {
            'solver': ('${SOLVER_LOG}', '--solver-data'),
            'watcher': ('${WATCHER_LOG}', '--watcher-data'),
            'variables': ('${VAR_FILE}', '--var'),
        }
        for kind, stream in payload['streams'].items():
            if stream['exists']:
                stream['explanation'] = ('Saved, but empty. This can be normal if nothing was written.'
                                         if not stream['size'] else 'Saved output.')
            elif kind in expected:
                destination, option = expected[kind]
                if not command:
                    stream['explanation'] = 'Not saved. The limiter command is unavailable for this older job, so its cause cannot be determined.'
                elif destination not in command or option not in command:
                    stream['explanation'] = f'Not captured: the configured limiter command does not direct {option} to {destination}. Check the workflow invocation.'
                else:
                    stream['explanation'] = 'Expected but missing: the limiter may have failed before writing this file. Inspect controller diagnostics; if those show no cause, this may be a capture bug.'
            else:
                stream['explanation'] = 'Unexpectedly missing from the saved result archive. This stream is captured by Slurmy; inspect controller diagnostics or report a capture bug.'
        return jsonify(payload)

    @app.get('/api/jobs/<job_id>/problems/<int:task_id>')
    def problem_data(job_id, task_id):
        job = job_snapshot(job_id, force=request.args.get('fresh') == '1')
        definition = job.tasks.get(task_id)
        if not definition:
            abort(404, 'Solver-problem pair not found in this job.')
        path = definition.problem
        ids=[task_id for task_id,definition in job.tasks.items() if definition.problem==path]
        page_number,per_page,sort,direction=call_page_args('wall')
        rows,matched=task_page(job,page_number,per_page,sort,direction,task_ids=ids)
        key=(host(),job_id,path)
        cached=problem_text_cache.get(key)
        if cached and request.args.get('fresh')!='1' and time.monotonic()-cached[0]<300:
            content,truncated=cached[1:]
        else:
            content,truncated=app.config.get('COLLECTOR',RemoteCollector)(host(),10).fetch_problem_text(job_id,path)
            problem_text_cache[key]=(time.monotonic(),content,truncated)
            if len(problem_text_cache)>32:problem_text_cache.pop(next(iter(problem_text_cache)))
        return jsonify(problem=path,text=content,truncated=truncated,total=job.task_count,percent=job.percent,tasks=rows,matched=matched)

    @app.post('/api/validate-path')
    def validate_path():
        data = request.get_json()
        if data.get('kind') == 'glob':
            if data.get('role') == 'axiom':
                pattern = str(workflows.path_at(data.get('value'), workflows.BASE))
                from slurmy_archives import expand_axiom_pattern
                matches = [str(path) for path, _ in expand_axiom_pattern(pattern, materialize=False)]
            else:
                pattern, matches = workflows.problem_glob(data.get('value'), materialize=False, host=host())
            return jsonify(path=pattern, count=len(matches), sample=matches[:5])
        value = workflows.existing(data.get('value'), data.get('kind'))
        return jsonify(path=str(value))

    @app.post('/api/validate-threads')
    def validate_threads():
        from check_threads import check_requested
        data = request.get_json() or {}
        requested = data.get('threads_per_core')
        if isinstance(requested, bool) or not str(requested).isdecimal() or int(requested) < 1:
            raise ValueError('Threads per core must be a positive whole number.')
        supported = check_requested(workflows.alias(data.get('host') or host()),
                                    workflows.alias(data.get('partition')), int(requested))
        return jsonify(supported=supported)

    @app.post('/api/suggest-batch-size')
    def suggest_batch_size():
        from .batch_suggestion import suggest
        return jsonify(suggest(request.get_json() or {}))

    @app.post('/api/validate-solver-cores')
    def validate_solver_cores():
        from check_threads import check_cores
        data = request.get_json() or {}
        requested = data.get('cores')
        if isinstance(requested, bool) or not str(requested).isdecimal() or int(requested) < 1:
            raise ValueError('Physical cores must be a positive whole number.')
        selected_host = str(data.get('host') or host())
        selected_partition = str(data.get('partition') or '')
        supported = check_cores(selected_host, selected_partition, int(requested))
        return jsonify(max_cores=supported)

    @app.post('/api/browse-tptp')
    def browse_tptp():
        from slurmy_archives import browse_cluster_tptp
        data = request.get_json() or {}
        selected_host = workflows.alias(data.get('host') or host())
        return jsonify(browse_cluster_tptp(data.get('directory') or 'tptp://', selected_host))

    @app.post('/api/browse-path')
    def browse_path():
        data = request.get_json() or {}
        path = Path(data.get('directory') or workflows.BASE).expanduser()
        path = (workflows.BASE / path).resolve() if not path.is_absolute() else path.resolve()
        if not path.is_dir():
            raise ValueError(f'Not an existing directory: {path}')
        try:
            children = sorted((item for item in path.iterdir() if not item.name.startswith('.')),
                              key=lambda item: (not item.is_dir(), not is_archive(item), item.name.casefold()))
        except PermissionError:
            raise ValueError(f'Permission denied: {path}')
        items = []
        for item in children[:1000]:
            try:
                if item.is_dir() or item.is_file():
                    items.append(dict(name=item.name, path=str(item), directory=item.is_dir()))
            except OSError:
                continue
        return jsonify(directory=str(path), parent=str(path.parent), items=items,
                       truncated=len(children) > 1000)

    def builder_stream(work):
        """Stream local builder progress without blocking its HTTP response."""
        events = queue.Queue()
        last_update = [0.0]
        operation_id = secrets.token_urlsafe(18)
        operation = {'cancel': threading.Event(), 'committing': False}
        with builder_lock:
            builder_operations[operation_id] = operation
        events.put(dict(type='started', id=operation_id))

        def progress(phase, path, completed, total):
            with builder_lock:
                if operation['cancel'].is_set():
                    raise BuilderCancelled()
                if phase == 'committing':
                    operation['committing'] = True
            now = time.monotonic()
            if phase in ('extracting', 'scanning') and completed not in (0, total) and now - last_update[0] < 0.15:
                return
            last_update[0] = now
            events.put(dict(type='progress', phase=phase, path=path,
                            completed=completed, total=total))

        def run():
            try:
                events.put(dict(type='result', **work(progress)))
            except BuilderCancelled:
                events.put(dict(type='cancelled'))
            except Exception as exc:
                events.put(dict(type='error', error=str(exc)))
            finally:
                with builder_lock:
                    builder_operations.pop(operation_id, None)

        threading.Thread(target=run, daemon=True).start()

        def output():
            try:
                while True:
                    try:
                        event = events.get(timeout=15)
                    except queue.Empty:
                        yield json.dumps({'type': 'heartbeat'}) + '\n'
                        continue
                    yield json.dumps(event) + '\n'
                    if event['type'] in ('result', 'error', 'cancelled'):
                        break
            finally:
                operation['cancel'].set()

        return Response(output(), mimetype='application/x-ndjson',
                        headers={'X-Accel-Buffering': 'no'})

    @app.post('/api/builder/cancel')
    def cancel_builder():
        operation_id = (request.get_json() or {}).get('id')
        if not isinstance(operation_id, str):
            raise ValueError('Choose a workflow preparation to cancel.')
        with builder_lock:
            operation = builder_operations.get(operation_id)
            if not operation or operation['committing']:
                return jsonify(cancelled=False)
            operation['cancel'].set()
        return jsonify(cancelled=True)

    def preview_files(data, target, progress=None):
        destination, files, count = workflows.specification(data, target, progress)
        preview_limit = 40_000
        visible = {name: body if len(body) <= preview_limit else
                   body[:preview_limit] + '\n… Preview truncated; the complete file will be saved.\n'
                   for name, body in files.items()}
        return dict(directory=str(destination), count=count, files=visible)

    @app.post('/api/preview')
    def preview():
        target = directory() if request.args.get('directory') else None
        data = request.get_json()
        if request.args.get('stream') == '1':
            return builder_stream(lambda progress: preview_files(data, target, progress))
        return jsonify(preview_files(data, target))

    @app.post('/api/workflows')
    @app.post('/api/experiments')
    def save():
        data = request.get_json()
        if request.args.get('stream') == '1':
            def work(progress):
                with guard:
                    destination, count = workflows.save(data, progress=progress)
                return dict(directory=str(destination), count=count)
            return builder_stream(work)
        with guard:
            destination, count = workflows.save(data)
        return jsonify(directory=str(destination), count=count), 201

    @app.post('/api/workflows/update')
    def update_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Duplicate an example before editing it.')
        data = request.get_json() or {}
        if request.args.get('stream') == '1':
            def work(progress):
                with guard:
                    destination, count = workflows.rename_and_save(data, path, progress)
                return dict(directory=str(destination), count=count)
            return builder_stream(work)
        with guard:
            destination, count = workflows.rename_and_save(data, path)
        return jsonify(directory=str(destination), count=count)

    @app.post('/api/workflows/name')
    def rename_workflow():
        path=directory()
        with guard:
            if any(op['scope']==str(path) and op['state']=='running' for op in operations.values()):
                abort(409,'Wait for the active workflow operation to finish before renaming it.')
            destination=workflows.rename_workflow(path,(request.get_json() or {}).get('name'))
        return jsonify(directory=str(destination),name=destination.name)

    @app.post('/api/workflows/duplicate')
    def duplicate_workflow():
        source = directory()
        with guard:
            destination = workflows.duplicate(source, (request.get_json() or {}).get('name'))
        return jsonify(directory=str(destination)), 201

    @app.post('/api/workflows/export')
    def export_workflow():
        from .portable import export_bundle
        path = directory()
        with guard:
            if any(op['scope'] == str(path) and op['state'] == 'running' for op in operations.values()):
                abort(409, 'Wait for the active workflow operation to finish before downloading it.')
        bundle = export_bundle(path, host())
        response = send_file(bundle, mimetype='application/gzip', as_attachment=True,
                             download_name=path.name + '.slurmy.tgz')
        response.call_on_close(bundle.close)
        return response

    @app.post('/api/workflows/import')
    def import_workflow():
        from .portable import import_bundle
        upload = request.files.get('workflow')
        if upload is None:
            raise ValueError('Choose a portable workflow .tgz file.')
        try:
            destination, count = import_bundle(upload.stream, request.form.get('name', ''))
        except (tarfile.TarError, UnicodeError, json.JSONDecodeError) as exc:
            raise ValueError('Invalid portable workflow archive.') from exc
        return jsonify(directory=str(destination), count=count), 201

    @app.get('/api/workflows/recovery')
    def recovery_list():
        from . import recovery
        return jsonify(entries=recovery.entries())

    @app.get('/api/workflows/recovery/<archive_id>')
    def recovery_inspect(archive_id):
        from . import recovery
        return jsonify(recovery.inspect(archive_id))

    @app.post('/api/workflows/recovery/<archive_id>')
    def recovery_action(archive_id):
        from . import recovery
        options=request.get_json() or {}
        with guard:
            _,item=recovery.entry(archive_id)
            if options.get('action')=='restore':
                destination=recovery.restore(archive_id)
                return jsonify(directory=str(destination),name=item['name'])
            if options.get('action')=='remove' and options.get('name')==item['name']:
                recovery.remove(archive_id)
                return jsonify(removed=item['name'])
            raise ValueError('Choose Restore or confirm the archived workflow name for permanent removal.')

    @app.post('/api/workflows/delete')
    def delete_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Example workflows cannot be deleted. Duplicate one to customize it.')
        expected = path.name.removeprefix('example-')
        options = request.get_json() or {}
        if not isinstance(options.get('permanent', False), bool):
            raise ValueError('Permanent deletion must be true or false.')
        if options.get('name') != expected:
            raise ValueError('Workflow name does not match the workflow being deleted.')
        with guard:
            if any(op['scope'] == str(path) and op['state'] == 'running' for op in operations.values()):
                raise ValueError('Wait for the active workflow operation to finish before deleting it.')
            archived = workflows.delete_workflow(path, permanent=options.get('permanent', False))
        return jsonify(deleted=expected, permanent=options.get('permanent', False),
                       archived=str(archived) if archived else None)

    def dispatch_metadata(operation, cwd):
        """Never show a previous preparation's count for a newly prepared job."""
        metadata = cwd / 'submit.sh.files' / 'metadata.json'
        try:
            if (operation['label'] == 'prepare and dispatch SLURM job'
                    and not operation.get('preparation_complete')
                    and metadata.stat().st_mtime < operation['started']):
                return {}
            return json.loads(metadata.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError):
            return {}

    def announce_operation_job(operation, job_id, cwd, environment):
        """Expose a dispatched job as soon as its ID appears in the operation log.

        The remote collector may not see the new job directory/metadata until a
        later poll.  Keeping this short-lived local record makes the submission
        visible immediately in both Job history and the originating workflow.
        """
        operation['preparation_complete'] = True
        details = dispatch_metadata(operation, cwd)
        directory = str(details.get('workflow_directory', cwd))
        with guard:
            provisional = operation.get('provisional_job_id')
            if provisional:
                announced_jobs.pop((environment['SLURMY_HOST'], provisional), None)
            announced_jobs[(environment['SLURMY_HOST'], job_id)] = dict(
                id=job_id, name=operation.get('requested_name') or Path(directory).name, state='SUBMITTED',
                total=int(details.get('task_count', 0) or 0), completed=0,
                percent=0.0, issues=0, created=time.time(), directory=directory,
                statuses={}, provisional=False)
            operation['job_id'] = job_id

    def announce_dispatching(operation, cwd, environment):
        """Show a dispatching row while submit.sh is still running."""
        details = dispatch_metadata(operation, cwd)
        provisional = f"dispatching-{operation['id']}"
        directory = str(details.get('workflow_directory', cwd))
        operation['provisional_job_id'] = provisional
        with guard:
            announced_jobs[(environment['SLURMY_HOST'], provisional)] = dict(
                id=provisional, name=operation.get('requested_name') or Path(directory).name, state='DISPATCHING',
                total=int(details.get('task_count', 0) or 0), completed=0,
                percent=0.0, issues=0, created=time.time(), directory=directory,
                statuses={}, provisional=True)

    def clear_dispatching(operation):
        provisional = operation.get('provisional_job_id')
        if not provisional:
            return
        with guard:
            announced_jobs.pop((operation['operation_host'], provisional), None)

    def operation(argv, cwd, environment, label, scope, requested_name=None):
        with guard:
            if app.config.get('SHUTTING_DOWN'):
                abort(409, 'The server is stopping. Reopen the app to continue.')
            active = [op for op in operations.values()
                      if op['scope'] == scope and op['state'] == 'running']
            if label == 'delete':
                deleting = next((op for op in active if op['label'] == 'delete'), None)
                if deleting:
                    return jsonify(deleting), 202
                dependencies = list(active)
                active = []
            else:
                dependencies = []
            if label == 'stop':
                # Double clicks and requests from other tabs must join the
                # existing stop, rather than report it as a conflicting action.
                stopping = next((op for op in active if op['label'] == 'stop'), None)
                if stopping:
                    return jsonify(stopping), 202
                active = [op for op in active if op['label'] not in {'sync', 'sync_verbose', 'resume'}]
            if active:
                active_label = {'sync': 'download', 'sync_verbose': 'verbose download'}.get(active[0]['label'], active[0]['label'])
                abort(409, f"Wait for the active {active_label} operation to finish.")
            key = secrets.token_hex(12)
            STATE.mkdir(exist_ok=True)
            log = STATE / (key + '.log')
            operations[key] = dict(id=key, label=label, scope=scope, state='running',
                                   code=None, started=time.time(), operation_cwd=str(cwd),
                                   operation_host=environment.get('SLURMY_HOST', 'datalab'),
                                   requested_name=requested_name)
            show_dispatching = label in {'prepare and dispatch SLURM job', 'dispatch prepared SLURM job'}
        if show_dispatching:
            announce_dispatching(operations[key], cwd, environment)
        def run():
            try:
                with log.open('w') as stream:
                    for previous in dependencies:
                        stream.write(f"Waiting for {previous['label']} to finish before stopping and deleting…\n")
                        stream.flush()
                        deadline = time.monotonic() + 300
                        while previous['state'] == 'running':
                            if time.monotonic() >= deadline:
                                raise RuntimeError('The previous operation has not finished; job was kept. Retry Delete when it completes.')
                            time.sleep(0.2)
                    code = subprocess.run(argv, cwd=cwd, env=environment, stdout=stream, stderr=subprocess.STDOUT).returncode
                operations[key].update(state='done' if code == 0 else 'failed', code=code)
                if label == 'delete' and code == 0:
                    with guard:
                        announced_jobs.pop((environment['SLURMY_HOST'], scope.split(':', 1)[1]), None)
                match = re.search(r'^Slurmy job ID: ([A-Za-z0-9._-]+)$', log.read_text(encoding='utf-8', errors='replace'), re.MULTILINE)
                if match and show_dispatching:
                    announce_operation_job(operations[key], match.group(1), cwd, environment)
                else:
                    clear_dispatching(operations[key])
            except Exception as exc:
                with log.open('a') as stream:
                    stream.write(f'\nOperation error: {exc}\n')
                operations[key].update(state='failed', code=-1)
                clear_dispatching(operations[key])
            with guard:
                cache.clear()
        threading.Thread(target=run, daemon=True).start()
        return jsonify(operations[key]), 202

    @app.post('/api/workflow/action')
    @app.post('/api/experiment/action')
    def experiment_action():
        path = directory()
        data = request.get_json() or {}
        action = data.get('action')
        if action not in {'all', 'prepare', 'prepare-submit', 'submit'}:
            raise ValueError('Unknown workflow action.')
        environment = {**os.environ, 'SLURMY_HOST': host(),
                       'PATH': str(Path(sys.executable).parent) + os.pathsep + os.environ.get('PATH', '')}
        # Makefiles are trusted user workflows; only this explicit click executes them.
        target = 'prepare' if action == 'all' else action
        label = {'prepare': 'prepare SLURM submission',
                 'prepare-submit': 'prepare and dispatch SLURM job',
                 'submit': 'dispatch prepared SLURM job'}[target]
        requested_name = display_name(data['name']) if target != 'prepare' and data.get('name', '').strip() else None
        if requested_name and target == 'submit' and (path / 'submit.sh').is_file() and \
                'JOB_NAME_B64' not in (path / 'submit.sh').read_text(encoding='utf-8'):
            raise ValueError('Prepared submission files predate job naming. Use Prepare & dispatch job to refresh them.')
        if requested_name:
            environment['SLURMY_JOB_NAME'] = requested_name
        else:
            environment.pop('SLURMY_JOB_NAME', None)
        return operation(['make', target, 'SLURMY_HOST=' + host()], path, environment, label, str(path), requested_name)

    @app.post('/api/jobs/<job_id>/action')
    def job_action(job_id):
        job = job_snapshot(job_id)
        data = request.get_json()
        if data.get('action') in ('sync', 'sync_verbose'):
            argv = [sys.executable, str(REPO/'implementation/slurmy-sync.py'), job_id, '--host', host()]
            if data['action'] == 'sync_verbose':
                argv.append('--verbose')
        elif data.get('action') in ('stop', 'resume'):
            argv = [sys.executable, str(REPO/'implementation/slurmy-job.py'),
                    data['action'], safe_job(job_id), '--host', host()]
        elif data.get('action') == 'cancel':
            slurm_id = str(data.get('slurm_id', ''))
            valid = {r.array_job_id for r in job.active_records}
            if slurm_id not in valid or not slurm_id.isdigit():
                raise ValueError('Choose an active SLURM ID from this job.')
            argv = [sys.executable, str(REPO/'implementation/slurmy-cancel.py'), slurm_id, '--host', host()]
        else:
            raise ValueError('Unknown job action.')
        return operation(argv, REPO, {**os.environ, 'SLURMY_HOST': host()}, data['action'], host() + ':' + job_id)

    @app.post('/api/jobs/<job_id>/delete')
    def delete_job(job_id):
        job_id = safe_job(job_id)
        # Deletion is an observable background operation: stop, wait, then remove.
        delete_local = bool((request.get_json() or {}).get('delete_local'))
        argv = [sys.executable, str(REPO/'implementation/slurmy-job.py'),
                'delete', job_id, '--host', host()]
        if delete_local:
            argv.append('--delete-local')
        return operation(argv, REPO, {**os.environ, 'SLURMY_HOST': host()},
                         'delete', host() + ':' + job_id)

    @app.get('/api/jobs/<job_id>/delete-options')
    def job_delete_options(job_id):
        job_id = safe_job(job_id)
        local = REPO / 'job-results' / job_id
        verbose_local = REPO / 'job-results' / 'verbose' / job_id
        if local.is_symlink() or verbose_local.is_symlink():
            abort(409, 'The local result directory is a symlink; remove it manually.')
        metadata = verbose_local / 'sync-metadata.json'
        if not metadata.is_file():
            metadata = local / 'sync-metadata.json'
        summary = {}
        if metadata.is_file():
            try:
                summary = json.loads(metadata.read_text(encoding='utf-8'))
            except (OSError, ValueError):
                pass
        percent = summary.get('percent_complete')
        calls = next(iter(sorted(local.glob('*.csv'))), local / 'calls.csv')
        if calls.is_file():
            try:
                with calls.open(newline='', encoding='utf-8') as stream:
                    done = total = 0
                    for row in csv.DictReader(stream):
                        total += 1
                        done += row.get('complete', '').lower() == 'true'
                percent = 100 * done / total if total else 0
            except (OSError, UnicodeError, csv.Error):
                pass
        return jsonify(synced=local.is_dir() or verbose_local.is_dir(), percent_complete=percent,
                       updated_at=summary.get('updated_at'))

    @app.get('/api/operations/<key>')
    def operation_state(key):
        if key not in operations:
            abort(404, 'Operation not found (the web app may have restarted).')
        log = STATE / (key + '.log')
        content = ''
        if log.exists():
            with log.open('rb') as stream:
                stream.seek(max(0, log.stat().st_size - 128*1024))
                content = stream.read().decode('utf-8', 'replace')
        # `make submit` can still be building/fetching resources after sbatch
        # has already accepted the job. Register that ID during polling rather
        # than waiting for the whole operation to finish.
        match = re.search(r'^Slurmy job ID: ([A-Za-z0-9._-]+)$', content, re.MULTILINE)
        if match and operations[key].get('provisional_job_id'):
            operation = operations[key]
            if operation.get('job_id') != match.group(1):
                announce_operation_job(operation, match.group(1), Path(operation['operation_cwd']),
                                       {'SLURMY_HOST': operation['operation_host']})
        op = operations[key]
        phase = ({'sync': 'Download results', 'sync_verbose': 'Download complete/verbose results',
                  'stop': 'Stop job', 'resume': 'Resume unfinished solver-problem pairs',
                  'delete': 'Stop and delete job', 'cancel': 'Cancel SLURM task'}.get(op['label'])
                 or submission_phase(content))
        return jsonify(**op, log=content, phase=phase)

    @app.get('/api/workflow/active-operation')
    def active_workflow_operation():
        path = directory()
        with guard:
            active = [op for op in operations.values()
                      if op['scope'] == str(path) and op['state'] == 'running'
                      and op['operation_host'] == host()]
            latest = max(active, key=lambda op: op['started']) if active else None
        return jsonify(operation=latest['id'] if latest else None)

    @app.post('/api/shutdown')
    def shutdown():
        callback = app.config.get('SHUTDOWN')
        if not callback:
            abort(409, 'This server is managed externally.')
        with guard:
            if any(op['state'] == 'running' for op in operations.values()):
                abort(409, 'Wait for active build/submit/download operations before stopping the app.')
            app.config['SHUTTING_DOWN'] = True
        threading.Timer(0.3, callback).start()
        return jsonify(stopped=True)

    return app
