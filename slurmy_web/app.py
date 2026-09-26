"""Flask routes for the single-user, loopback-only web application."""
from __future__ import annotations

from dataclasses import asdict
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import threading
import time
from datetime import datetime

from flask import Flask, abort, jsonify, redirect, render_template, request, session, send_file, url_for
from werkzeug.exceptions import HTTPException

from .monitor import RemoteCollector
from . import workflows

REPO = workflows.REPO
STATE = REPO / '.slurmy-web'


def safe_job(value):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', value):
        raise ValueError('Invalid job ID.')
    return value


def summary(job):
    workflow_directory = job.metadata.get('workflow_directory', '')
    workflow_name = Path(workflow_directory).name if workflow_directory else ''
    if workflow_name.startswith('example-') and Path(workflow_directory).parent == workflows.USER_RUNS:
        workflow_name = workflow_name.removeprefix('example-')
    return dict(id=job.job_id, name=workflow_name or job.job_name, state=job.state,
                total=job.task_count, completed=job.effective_completed,
                percent=job.percent, issues=job.issue_count, created=job.created_epoch,
                directory=workflow_directory,
                statuses=job.status_counts)


def task_rows(job):
    for task_id in sorted(set(job.tasks) | set(job.results)):
        definition = job.tasks.get(task_id)
        result = job.results.get(task_id)
        progress = job.call_progress.get(task_id)
        state = 'pending' if job.active_records else 'not run'
        wall = None
        if result:
            state, wall = result.status, result.wall_seconds
        elif progress and progress.batch_id in job.active_batch_ids:
            state = progress.state
            wall = max(0, time.time() - progress.started_epoch) if progress.started_epoch else None
        else:
            # Historical sequential-batch jobs remain inspectable.
            progress = job.progress.get(task_id // job.batch_size)
            if progress and progress.task_id == task_id and progress.batch_id in job.active_batch_ids:
                state = progress.state
                wall = max(0, time.time() - progress.started_epoch) if progress.started_epoch else None
        yield dict(id=task_id, system=definition.system if definition else '',
                   problem=definition.problem if definition else '',
                   command=definition.command if definition else '',
                   limiter_command=definition.limiter_command if definition else '',
                   directory=definition.solver_root if definition else '',
                   state=state, wall=wall, cpu=result.cpu_seconds if result else None,
                   memory=result.max_memory_kib * 1024 if result and result.max_memory_kib is not None else None,
                   return_code=result.return_code if result else '',
                   output=bool(result and result.archive))


def create_app():
    workflows.migrate_user_runs()
    app = Flask(__name__)
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
        return jsonify(app='slurmy-web', repository=str(REPO), pid=os.getpid())

    @app.get('/')
    def index():
        return page('jobs.html')

    @app.get('/jobs/<job_id>')
    def job_page(job_id):
        return page('job.html', job_id=safe_job(job_id))

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
                 ('jobpairs.csv', 'configurations.csv', 'building.txt', 'resource_limiter_template.txt', 'Makefile') if (path / name).is_file()}
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
        local = [item for (item_host, _), item in announced if item_host == host()]
        if local and not cached:
            items = local
            updated = time.time()
        else:
            value = snapshot()
            items = [summary(job) for job in value.jobs]
            updated = value.fetched_epoch
        remote_ids = {item['id'] for item in items}
        items.extend(item for (item_host, job_id), item in announced
                     if item_host == host() and job_id not in remote_ids)
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
        page_number = max(0, int(request.args.get('page', '0')))
        return jsonify(**summary(job), tasks=rows[page_number*100:(page_number+1)*100], matched=len(rows))

    @app.get('/api/jobs/<job_id>/output/<int:task_id>')
    def output(job_id, task_id):
        job = job_snapshot(job_id)
        result = job.results.get(task_id)
        if not result or not result.archive:
            abort(404, 'Output is available after this call saves its result archive.')
        value = app.config.get('COLLECTOR', RemoteCollector)(host(), 10).fetch_task_output(job_id, result)
        return jsonify(asdict(value))

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
            raise ValueError(f'Type {expected} exactly to confirm deletion.')
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
                id=job_id, name=Path(directory).name, state='SUBMITTED',
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
                id=provisional, name=Path(directory).name, state='DISPATCHING',
                total=int(details.get('task_count', 0) or 0), completed=0,
                percent=0.0, issues=0, created=time.time(), directory=directory,
                statuses={}, provisional=True)

    def clear_dispatching(operation):
        provisional = operation.get('provisional_job_id')
        if not provisional:
            return
        with guard:
            announced_jobs.pop((operation['operation_host'], provisional), None)

    def operation(argv, cwd, environment, label, scope):
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
                                   operation_host=environment.get('SLURMY_HOST', 'datalab'))
            show_dispatching = label == 'build resources & dispatch Slurm job'
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
        action = request.get_json().get('action')
        if action not in {'all', 'submit'}:
            raise ValueError('Unknown workflow action.')
        environment = {**os.environ, 'SLURMY_HOST': host(),
                       'PATH': str(Path(sys.executable).parent) + os.pathsep + os.environ.get('PATH', '')}
        # Makefiles are trusted user workflows; only this explicit click executes them.
        label = 'prepare Slurm submission' if action == 'all' else 'build resources & dispatch Slurm job'
        return operation(['make', action, 'SLURMY_HOST=' + host()], path, environment, label, str(path))

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
        return jsonify(**operations[key], log=content)

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
