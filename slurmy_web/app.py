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

from flask import Flask, abort, jsonify, render_template, request, session, send_file
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
    return dict(id=job.job_id, name=job.job_name, state=job.state,
                total=job.task_count, completed=job.effective_completed,
                percent=job.percent, issues=job.issue_count, created=job.created_epoch,
                directory=job.metadata.get('workflow_directory', ''),
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
                   directory=definition.solver_root if definition else '',
                   state=state, wall=wall, cpu=result.cpu_seconds if result else None,
                   memory=result.max_memory_kib * 1024 if result and result.max_memory_kib is not None else None,
                   return_code=result.return_code if result else '',
                   output=bool(result and result.archive))


def create_app():
    app = Flask(__name__)
    app.config.update(SECRET_KEY=secrets.token_hex(32), MAX_CONTENT_LENGTH=2 * 1024 * 1024,
                      SESSION_COOKIE_HTTPONLY=True, SESSION_COOKIE_SAMESITE='Strict',
                      TRUSTED_HOSTS=['127.0.0.1', 'localhost'])
    cache, locks, operations = {}, {}, {}
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
        path = workflows.existing(request.args.get('directory'), 'directory').resolve()
        allowed = [REPO / 'ExampleRuns', workflows.GENERATED]
        if not any(path.is_relative_to(root.resolve()) and path != root.resolve() for root in allowed):
            raise ValueError('Choose a workflow under ExampleRuns or YourRuns/GENERATED.')
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

    @app.get('/experiments')
    def experiments():
        entries = []
        for root in (REPO / 'ExampleRuns', workflows.GENERATED):
            if not root.exists():
                continue
            for path in sorted(root.iterdir()):
                if path.is_dir() and (path / 'Makefile').is_file():
                    entries.append(dict(name=path.name, directory=str(path.resolve()), example=root.name == 'ExampleRuns'))
        return page('experiments.html', experiments=entries)

    @app.get('/experiment')
    def experiment():
        path = directory()
        files = {name: (path / name).read_text() for name in
                 ('jobpairs.csv', 'configurations.csv', 'building.txt', 'resource_limiter_template.txt', 'Makefile') if (path / name).is_file()}
        return page('experiment.html', directory=str(path), name=path.name, files=files,
                    prepared=(path / 'submit.sh').is_file())

    @app.get('/new')
    def new():
        return page('new.html', base=str(workflows.BASE))

    @app.get('/logo.svg')
    def logo():
        return send_file(REPO / 'logo.svg')

    @app.get('/api/jobs')
    def jobs():
        value = snapshot()
        items = [summary(job) for job in value.jobs]
        folder = request.args.get('directory')
        if folder:
            items = [job for job in items if job['directory'] == folder]
        return jsonify(jobs=items, updated=value.fetched_epoch)

    @app.get('/api/jobs/<job_id>')
    def job_data(job_id):
        job = job_snapshot(job_id)
        query = request.args.get('q', '').lower()
        rows = [row for row in task_rows(job) if query in ' '.join(str(v) for v in row.values()).lower()]
        page_number = max(0, int(request.args.get('page', '0')))
        return jsonify(**summary(job), tasks=rows[page_number*100:(page_number+1)*100], matched=len(rows),
                       slurm=[asdict(record) for record in job.slurm], logs=job.logs, metadata=job.metadata)

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

    @app.post('/api/preview')
    def preview():
        destination, files, count = workflows.specification(request.get_json())
        return jsonify(directory=str(destination), count=count, files=files)

    @app.post('/api/experiments')
    def save():
        with guard:
            destination, count = workflows.save(request.get_json())
        return jsonify(directory=str(destination), count=count), 201

    def operation(argv, cwd, environment, label, scope):
        with guard:
            if app.config.get('SHUTTING_DOWN'):
                abort(409, 'The server is stopping. Reopen the app to continue.')
            if any(op['scope'] == scope and op['state'] == 'running' for op in operations.values()):
                abort(409, 'An operation for this workflow/job is already running.')
            key = secrets.token_hex(12)
            STATE.mkdir(exist_ok=True)
            log = STATE / (key + '.log')
            operations[key] = dict(id=key, label=label, scope=scope, state='running', code=None, started=time.time())
        def run():
            try:
                with log.open('w') as stream:
                    code = subprocess.run(argv, cwd=cwd, env=environment, stdout=stream, stderr=subprocess.STDOUT).returncode
                operations[key].update(state='done' if code == 0 else 'failed', code=code)
            except Exception as exc:
                log.write_text(str(exc))
                operations[key].update(state='failed', code=-1)
            with guard:
                cache.clear()
        threading.Thread(target=run, daemon=True).start()
        return jsonify(operations[key]), 202

    @app.post('/api/experiment/action')
    def experiment_action():
        path = directory()
        action = request.get_json().get('action')
        if action not in {'all', 'submit'}:
            raise ValueError('Unknown workflow action.')
        environment = {**os.environ, 'SLURMY_HOST': host(),
                       'PATH': str(Path(sys.executable).parent) + os.pathsep + os.environ.get('PATH', '')}
        # Makefiles are trusted user workflows; only this explicit click executes them.
        return operation(['make', action, 'SLURMY_HOST=' + host()], path, environment, action, str(path))

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
