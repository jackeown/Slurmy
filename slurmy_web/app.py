"""Flask routes for the single-user, loopback-only web application."""
from __future__ import annotations

from dataclasses import asdict
from collections import Counter
import base64
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime

from flask import Flask, abort, jsonify, redirect, render_template, request, session, send_file, url_for
from werkzeug.exceptions import HTTPException

from .monitor import RemoteCollector
from .launcher import web_version
from . import workflows

REPO = workflows.REPO
STATE = REPO / '.slurmy-web'


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
    workflow_directory = job.metadata.get('workflow_directory', '')
    workflow_name = Path(workflow_directory).name if workflow_directory else ''
    if workflow_name.startswith('example-') and Path(workflow_directory).parent == workflows.USER_RUNS:
        workflow_name = workflow_name.removeprefix('example-')
    return dict(id=job.job_id, name=job.metadata.get('job_name') or workflow_name or job.job_name, state=job.state,
                total=job.task_count, completed=job.effective_completed,
                percent=job.percent, issues=job.issue_count, created=job.created_epoch,
                directory=workflow_directory,
                submission_state=job.submission_state, submitted_batches=job.submission_count,
                batch_count=job.batch_count,
                statuses=job.status_counts)


def outcome_breakdown(job):
    """Count final SZS answers or fallback execution outcomes across all calls."""
    fallback = {'ok': 'OK', 'time-limit': 'Time limit',
                'counter-satisfiable': 'CounterSatisfiable',
                'memory-limit': 'Memory limit'}
    counts = Counter()
    for result in job.results.values():
        szs = result.szs_status.strip()
        if szs:
            label, source = szs, 'prover'
        else:
            label = fallback.get(result.status, result.status.replace('-', ' ').capitalize())
            source = ('resource limiter' if result.status in
                      {'time-limit', 'memory-limit', 'resource-limiter-error'} else
                      'controller' if result.status in {'worker-error', 'interrupted'} else
                      'execution')
        counts[(label, source)] += 1
    total = max(job.task_count, len(job.tasks), len(job.results))
    unfinished = max(0, total - len(job.results))
    if unfinished:
        counts[('Not finished', '')] += unfinished
    return [{'label': label, 'source': source, 'count': count}
            for (label, source), count in sorted(
                counts.items(), key=lambda item: (-item[1], item[0][0].lower(), item[0][1]))]


def submission_phase(log):
    """Summarize the newest visible step of a web-triggered submission."""
    phase = 'Preparing submission files…'
    for line in log.splitlines():
        if line.startswith(('Build progress: ', 'Submission phase: ', 'Submission waiting: ',
                            'Submission progress: ', 'Submission complete: ')):
            phase = line
        elif match := re.fullmatch(r'Batch \d+: Slurm \d+ \((\d+/\d+) accepted\)', line):
            phase = f'Submitting batches: {match.group(1)} accepted'
    return phase


def unfinished_call(job, task_id, progress):
    """Explain a missing result using this call's actual Slurm batch."""
    batch_id = job.task_batches.get(task_id)
    if batch_id is None and progress:
        batch_id = progress.batch_id
    if batch_id is None:
        batch_id = task_id // job.batch_size
    records = [record for record in job.slurm
               if record.array_task_id.isdigit()
               and job.submission_offsets.get(record.array_job_id, 0) + int(record.array_task_id) == batch_id]
    records.sort(key=lambda record: record.source != 'queue')
    record = records[0] if records else None
    log = ''
    if record:
        log = job.logs.get(f'slurm_{record.array_job_id}_{record.array_task_id}.out', '')
    excerpt = next((line.strip() for line in reversed(log.splitlines()) if line.strip()), '')[:240]
    suffix = f' Last Slurm log line: {excerpt}' if excerpt else ''
    if record and record.source == 'queue':
        code = record.state.upper().split()[0]
        if code in {'RUNNING', 'COMPLETING'}:
            if progress and progress.state == 'running':
                return 'running', 'Running', f'Call started in Slurm batch {batch_id}; no result saved yet.', log
            return 'batch starting', 'Yes', f'Slurm batch {batch_id} is {code.lower()}, but this call has not recorded a start yet.{suffix}', log
        return 'queued', 'Yes', f'Slurm batch {batch_id} is {code.lower()}: {record.location}.{suffix}', log
    if record:
        code = record.state.upper().split()[0].rstrip('+')
        label = {'COMPLETED': 'result missing', 'TIMEOUT': 'batch timed out',
                 'OUT_OF_MEMORY': 'batch out of memory', 'NODE_FAIL': 'node failed',
                 'CANCELLED': 'batch cancelled', 'FAILED': 'batch failed',
                 'PREEMPTED': 'batch preempted'}.get(code, 'batch ' + code.lower())
        reason = (f'Slurm batch {batch_id} ended as {code}, but this call has no saved result.'
                  f'{suffix} See its batch log for details.')
        return label, 'No', reason, log
    if progress and progress.state != 'running':
        return 'result missing', 'No', f'Call recorded {progress.state}, but no result was saved. Slurm no longer reports batch {batch_id}.', log
    if batch_id not in job.submitted_batches and job.submitted_batches:
        if job.submission_state == 'submitting':
            return 'awaiting submission', 'Yes', f'Batch {batch_id} is waiting for a free Slurm submission slot.', log
        return 'not submitted', 'No', f'Batch {batch_id} was planned but has no Slurm submission. Submission may have stopped partway through.', log
    if not job.submission_offsets and not job.slurm:
        recent = time.time() - job.created_epoch < 180
        return ('preparing submission', 'Unknown', 'No Slurm submission recorded yet; preparation may still be underway.', log) if recent else (
            'not submitted', 'No', 'No Slurm submission was recorded for this job.', log)
    return 'scheduler status unknown', 'Unknown', f'No current or historical Slurm record is available for batch {batch_id}; cannot tell whether it will run.', log


def task_rows(job):
    for task_id in sorted(set(job.tasks) | set(job.results)):
        definition = job.tasks.get(task_id)
        result = job.results.get(task_id)
        progress = job.call_progress.get(task_id)
        state = 'pending'
        will_run = ''
        reason = ''
        diagnostic = ''
        wall = None
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
            state, will_run, reason, diagnostic = unfinished_call(job, task_id, progress)
            if progress and progress.state == 'running' and state == 'running':
                wall = max(0, time.time() - progress.started_epoch) if progress.started_epoch else None
        yield dict(id=task_id, system=definition.system if definition else '',
                   problem=definition.problem if definition else '',
                   command=definition.command if definition else '',
                   limiter_command=definition.limiter_command if definition else '',
                   directory=definition.solver_root if definition else '',
                   state=state, wall=wall, cpu=result.cpu_seconds if result else None,
                   memory=result.max_memory_kib * 1024 if result and result.max_memory_kib is not None else None,
                   return_code=result.return_code if result else '',
                   execution_status=result.status if result else '',
                   szs_status=result.szs_status if result else '',
                   will_run=will_run, reason=reason, diagnostic=diagnostic,
                   output=bool(result and result.archive))


CALL_SORT_FIELDS = {'id', 'system', 'problem', 'state', 'will_run', 'reason', 'cpu', 'wall', 'memory'}


def sorted_call_rows(rows, field, direction):
    if field not in CALL_SORT_FIELDS or direction not in {'asc', 'desc'}:
        abort(400, 'Invalid call sort order.')
    # Keep absent measurements last in either direction, and IDs stable for ties.
    present = [row for row in rows if row[field] is not None and row[field] != '']
    missing = [row for row in rows if row[field] is None or row[field] == '']
    present.sort(key=lambda row: (row[field].casefold() if isinstance(row[field], str) else row[field], row['id']),
                 reverse=direction == 'desc')
    return present + missing


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
    guard = threading.Lock()

    @app.before_request
    def local_only():
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
        response.headers['Content-Security-Policy'] = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'"
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
            code, message = 500, 'Unexpected error. See .slurmy-web/server.log.'
        if request.path.startswith('/api/'):
            return jsonify(error=message), code
        return render_template('error.html', message=message), code

    def host():
        return workflows.alias(request.args.get('host') or os.environ.get('SLURMY_HOST', 'datalab'))

    def snapshot(detail=None):
        key = (host(), detail)
        with guard:
            lock = locks.setdefault(key, threading.Lock())
        with lock:
            cached = cache.get(key)
            if cached and time.monotonic() - cached[0] < 5:
                return cached[1]
            value = app.config.get('COLLECTOR', RemoteCollector)(key[0], 10).fetch(detail)
            cache[key] = (time.monotonic(), value)
            if len(cache) > 32:
                cache.pop(next(iter(cache)))
            return value

    def job_snapshot(job_id):
        job = snapshot(safe_job(job_id)).find_job(job_id)
        if not job:
            abort(404, 'Job not found on this SSH host.')
        return job

    def directory():
        requested = request.args.get('directory')
        if requested:
            old = workflows.path_at(requested, workflows.BASE)
            if old.parent == workflows.LEGACY_RUNS and (workflows.USER_RUNS / old.name).is_dir():
                requested = str(workflows.USER_RUNS / old.name)
        path = workflows.existing(requested, 'directory').resolve()
        allowed = [REPO / 'ExampleRuns', workflows.USER_RUNS]
        if not any(path.is_relative_to(root.resolve()) and path != root.resolve() for root in allowed):
            raise ValueError('Choose a workflow under ExampleRuns or YourRuns.')
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
                    example_workflows=entries(REPO / 'ExampleRuns'))

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
        return page('new.html', base=str(workflows.BASE), initial=None, editing=False)

    @app.get('/workflow/edit')
    def edit_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Duplicate an example before editing it.')
        return page('new.html', base=str(workflows.BASE), initial=workflows.decisions(path),
                    editing=True, directory=str(path))

    @app.get('/logo.svg')
    def logo():
        return send_file(REPO / 'logo.svg')

    @app.get('/api/jobs')
    def jobs():
        with guard:
            announced = list(announced_jobs.items())
            cached = cache.get((host(), None))
        # Once dispatch has started, do not make the user wait for the first
        # SSH snapshot just to see it.  A later poll merges in remote history.
        local = [item.copy() for (item_host, _), item in announced if item_host == host()]
        for item in local:
            if not item.get('provisional'):
                continue
            operation_key = item['id'].removeprefix('dispatching-')
            operation = operations.get(operation_key)
            log_path = STATE / (operation_key + '.log')
            if operation and log_path.is_file():
                item['phase'] = submission_phase(log_path.read_text(encoding='utf-8', errors='replace')[-16384:])
                item['state'] = 'DISPATCHING' if operation['state'] == 'running' else operation['state'].upper()
        if local and not cached:
            items = local
            updated = time.time()
        else:
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
        job = job_snapshot(job_id)
        query = request.args.get('q', '').lower()
        rows = [row for row in task_rows(job) if query in ' '.join(str(v) for v in row.values()).lower()]
        page_number, per_page, sort, direction = call_page_args()
        rows = sorted_call_rows(rows, sort, direction)
        return jsonify(**summary(job), outcomes=outcome_breakdown(job),
                       tasks=rows[page_number*per_page:(page_number+1)*per_page], matched=len(rows))

    @app.post('/api/jobs/<job_id>/name')
    def rename_job(job_id):
        job_id = safe_job(job_id)
        name = display_name((request.get_json() or {}).get('name', ''))
        job = job_snapshot(job_id)
        if job.active_records or job.submission_state == 'submitting':
            abort(409, 'Wait until every Slurm allocation for this job has finished before renaming its directory.')
        with guard:
            if any(op['state'] == 'running' and
                   (op['scope'] == host() + ':' + job_id or op.get('job_id') == job_id)
                   for op in operations.values()):
                abort(409, 'Wait for the active job operation to finish before renaming it.')
        new_id = renamed_job_id(job_id, name)
        local_old = REPO / 'slurmy-results' / job_id
        local_new = REPO / 'slurmy-results' / new_id
        if local_old.is_symlink() or (new_id != job_id and (local_new.exists() or local_new.is_symlink())):
            abort(409, 'The local result directory is a symlink or the new job ID already exists locally.')
        script = (REPO / 'templates/remote_rename_job.sh').read_text(encoding='utf-8')
        encoded = base64.b64encode(name.encode('utf-8')).decode('ascii')
        result = subprocess.run(['ssh', '-T', '-o', 'BatchMode=yes', '--', host(),
                                 'bash', '-s', '--', job_id, new_id, encoded],
                                input=script, text=True, capture_output=True, timeout=90, check=False)
        if result.returncode:
            abort(409, result.stderr.strip() or 'Could not rename the cluster job directory.')
        warning = ''
        try:
            if local_old.is_dir() and new_id != job_id:
                local_old.rename(local_new)
            local = local_new if new_id != job_id else local_old
            if local.is_dir():
                for filename in ('metadata.json', 'sync-metadata.json'):
                    path = local / filename
                    if not path.is_file():
                        continue
                    data = json.loads(path.read_text(encoding='utf-8'))
                    if filename == 'metadata.json':
                        history = data.get('previous_job_ids', [])
                        if job_id != new_id and job_id not in history:
                            history.append(job_id)
                        data.update(job_id=new_id, job_name=name, previous_job_ids=history)
                    else:
                        data.update(slurmy_job_id=new_id, remote_directory=f'$HOME/Slurmy/{new_id}',
                                    local_directory=str(local.resolve()))
                    temporary = path.with_suffix(path.suffix + '.tmp')
                    temporary.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
                    temporary.replace(path)
        except (OSError, ValueError, TypeError) as exc:
            warning = f'Cluster job renamed, but local synced results need attention: {exc}'
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
            abort(404, 'Output is available after this call saves its result archive.')
        value = app.config.get('COLLECTOR', RemoteCollector)(host(), 10).fetch_task_output(job_id, result)
        return jsonify(asdict(value))

    @app.get('/api/jobs/<job_id>/problems/<int:task_id>')
    def problem_data(job_id, task_id):
        job = job_snapshot(job_id)
        definition = job.tasks.get(task_id)
        if not definition:
            abort(404, 'Call not found in this job.')
        path = definition.problem
        rows = [row for row in task_rows(job) if row['problem'] == path]
        page_number, per_page, sort, direction = call_page_args('wall')
        rows = sorted_call_rows(rows, sort, direction)
        content, truncated = app.config.get('COLLECTOR', RemoteCollector)(host(), 10).fetch_problem_text(job_id, path)
        return jsonify(problem=path, text=content, truncated=truncated,
                       tasks=rows[page_number*per_page:(page_number+1)*per_page], matched=len(rows))

    @app.post('/api/validate-path')
    def validate_path():
        data = request.get_json()
        if data.get('kind') == 'glob':
            pattern, matches = workflows.problem_glob(data.get('value'))
            return jsonify(path=pattern, count=len(matches), sample=matches[:5])
        value = workflows.existing(data.get('value'), data.get('kind'))
        return jsonify(path=str(value))

    @app.post('/api/browse-path')
    def browse_path():
        data = request.get_json() or {}
        path = Path(data.get('directory') or workflows.BASE).expanduser()
        path = (workflows.BASE / path).resolve() if not path.is_absolute() else path.resolve()
        if not path.is_dir():
            raise ValueError(f'Not an existing directory: {path}')
        try:
            children = sorted(path.iterdir(), key=lambda item: (not item.is_dir(), item.name.casefold()))
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

    @app.post('/api/preview')
    def preview():
        target = directory() if request.args.get('directory') else None
        destination, files, count = workflows.specification(request.get_json(), target)
        return jsonify(directory=str(destination), count=count, files=files)

    @app.post('/api/workflows')
    @app.post('/api/experiments')
    def save():
        with guard:
            destination, count = workflows.save(request.get_json())
        return jsonify(directory=str(destination), count=count), 201

    @app.post('/api/workflows/update')
    def update_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Duplicate an example before editing it.')
        data = request.get_json() or {}
        with guard:
            destination, count = workflows.rename_and_save(data, path)
        return jsonify(directory=str(destination), count=count)

    @app.post('/api/workflows/duplicate')
    def duplicate_workflow():
        source = directory()
        with guard:
            destination = workflows.duplicate(source, (request.get_json() or {}).get('name'))
        return jsonify(directory=str(destination)), 201

    @app.post('/api/workflows/delete')
    def delete_workflow():
        path = directory()
        if path.parent != workflows.USER_RUNS.resolve():
            raise ValueError('Example workflows cannot be deleted. Duplicate one to customize it.')
        expected = path.name.removeprefix('example-')
        if (request.get_json() or {}).get('name') != expected:
            raise ValueError('Workflow name does not match the workflow being deleted.')
        with guard:
            if any(op['scope'] == str(path) and op['state'] == 'running' for op in operations.values()):
                raise ValueError('Wait for the active workflow operation to finish before deleting it.')
            archived = workflows.delete_workflow(path)
        return jsonify(deleted=expected, archived=str(archived))

    def announce_operation_job(operation, job_id, cwd, environment):
        """Expose a dispatched job as soon as its ID appears in the operation log.

        The remote collector may not see the new job directory/metadata until a
        later poll.  Keeping this short-lived local record makes the submission
        visible immediately in both Job history and the originating workflow.
        """
        metadata = cwd / 'submit.sh.files' / 'metadata.json'
        try:
            details = json.loads(metadata.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError):
            details = {}
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
        metadata = cwd / 'submit.sh.files' / 'metadata.json'
        try:
            details = json.loads(metadata.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError):
            details = {}
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
            if any(op['scope'] == scope and op['state'] == 'running' for op in operations.values()):
                abort(409, 'An operation for this workflow/job is already running.')
            key = secrets.token_hex(12)
            STATE.mkdir(exist_ok=True)
            log = STATE / (key + '.log')
            operations[key] = dict(id=key, label=label, scope=scope, state='running',
                                   code=None, started=time.time(), operation_cwd=str(cwd),
                                   operation_host=environment.get('SLURMY_HOST', 'datalab'),
                                   requested_name=requested_name)
            show_dispatching = label in {'prepare and dispatch Slurm job', 'dispatch prepared Slurm job'}
        if show_dispatching:
            announce_dispatching(operations[key], cwd, environment)
        def run():
            try:
                with log.open('w') as stream:
                    code = subprocess.run(argv, cwd=cwd, env=environment, stdout=stream, stderr=subprocess.STDOUT).returncode
                operations[key].update(state='done' if code == 0 else 'failed', code=code)
                match = re.search(r'^Slurmy job ID: ([A-Za-z0-9._-]+)$', log.read_text(encoding='utf-8', errors='replace'), re.MULTILINE)
                if match:
                    announce_operation_job(operations[key], match.group(1), cwd, environment)
                else:
                    clear_dispatching(operations[key])
            except Exception as exc:
                log.write_text(str(exc))
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
        label = {'prepare': 'prepare Slurm submission',
                 'prepare-submit': 'prepare and dispatch Slurm job',
                 'submit': 'dispatch prepared Slurm job'}[target]
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
        if data.get('action') == 'sync':
            argv = [sys.executable, str(REPO/'slurmy-sync.py'), job_id, '--host', host()]
        elif data.get('action') == 'cancel':
            slurm_id = str(data.get('slurm_id', ''))
            valid = {r.array_job_id for r in job.active_records}
            if slurm_id not in valid or not slurm_id.isdigit():
                raise ValueError('Choose an active Slurm ID from this job.')
            argv = [sys.executable, str(REPO/'slurmy-cancel.py'), slurm_id, '--host', host()]
        else:
            raise ValueError('Unknown job action.')
        return operation(argv, REPO, os.environ.copy(), data['action'], host() + ':' + job_id)

    @app.post('/api/jobs/<job_id>/delete')
    def delete_job(job_id):
        job_id = safe_job(job_id)
        job = job_snapshot(job_id)
        if job.active_records:
            abort(409, 'This job still has active Slurm allocations. Cancel them and wait for completion before deleting it.')
        with guard:
            if any(op['scope'] == host() + ':' + job_id and op['state'] == 'running' for op in operations.values()):
                abort(409, 'Wait for the active job operation to finish before deleting it.')
        local = REPO / 'slurmy-results' / job_id
        if local.is_symlink():
            abort(409, 'The local result directory is a symlink; remove it manually.')
        script = r'''set -euo pipefail
job="$HOME/Slurmy/$1"
[[ -d "$job" && ! -L "$job" ]] || { echo 'Job directory not found or is a symlink.' >&2; exit 2; }
if squeue -h -u "$USER" -o '%Z' | grep -Fxq -- "$job"; then
    echo 'Slurm still has an active allocation for this job.' >&2
    exit 3
fi
rm -r -- "$job"
'''
        result = subprocess.run(['ssh', '-T', '-o', 'BatchMode=yes', '--', host(), 'bash', '-s', '--', job_id],
                                input=script, text=True, capture_output=True, timeout=60, check=False)
        if result.returncode:
            abort(409, result.stderr.strip() or 'Could not delete the cluster job directory.')
        if local.is_dir():
            shutil.rmtree(local)
        with guard:
            announced_jobs.pop((host(), job_id), None)
            cache.clear()
        return jsonify(deleted=job_id)

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
        if match:
            operation = operations[key]
            if operation.get('job_id') != match.group(1):
                announce_operation_job(operation, match.group(1), Path(operation['operation_cwd']),
                                       {'SLURMY_HOST': operation['operation_host']})
        return jsonify(**operations[key], log=content, phase=submission_phase(content))

    @app.post('/api/shutdown')
    def shutdown():
        callback = app.config.get('SHUTDOWN')
        if not callback:
            abort(409, 'This server is managed externally.')
        with guard:
            if any(op['state'] == 'running' for op in operations.values()):
                abort(409, 'Wait for active build/submit/sync operations before stopping the app.')
            app.config['SHUTTING_DOWN'] = True
        threading.Timer(0.3, callback).start()
        return jsonify(stopped=True)

    return app
