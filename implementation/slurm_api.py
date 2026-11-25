"""Optional SLURM REST transport; credentials stay in process memory only."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

from ssh_transport import connection_options

DEFAULT_URL = 'http://slurmapi.datalab.tuwien.ac.at:6820'
DEFAULT_VERSION = 'v0.0.40'
CONFIG = Path(os.environ.get('SLURMY_CONNECTIONS_FILE', str(Path.home()/'.config/slurmy/connections.json')))
_lock = threading.RLock()
_credentials = {}
_reads = {}
_read_locks = {}
_account_limits = {}


def account_limit(host, account='', partition=''):
    """Cache accounting-policy reads; an unreported limit is not assumed to be 50."""
    if not account or account == 'default':
        return 50
    from submit_coordinator import association_limit
    import shlex
    key = (host, account, partition)
    with _lock:
        query_lock = _read_locks.setdefault(('account-policy', *key), threading.Lock())
    with query_lock:
        cached = _account_limits.get(key)
        if cached and time.monotonic()-cached[0] < 300:
            return cached[1]
        command = 'sacctmgr -nP show assoc user="$USER" ' + shlex.join([f'account={account}', 'format=User,Account,Partition,MaxSubmitJobs'])
        try:
            result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--', host, command],
                                    capture_output=True, text=True, timeout=20, check=False)
            limit = association_limit(result.stdout, account, partition) if result.returncode == 0 else None
        except (OSError, subprocess.TimeoutExpired):
            limit = None
        _account_limits[key] = (time.monotonic(), limit)
        return limit
_submission_metadata = {}


def settings(host):
    try:
        values = json.loads(CONFIG.read_text()).get(host, {})
    except FileNotFoundError:
        values = {}
    return dict(mode=values.get('mode', 'ssh'), url=values.get('url', DEFAULT_URL),
                version=values.get('version', DEFAULT_VERSION),
                submission='laptop')


def save_settings(host, values):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', host):
        raise ValueError('Invalid SSH host alias.')
    mode = values.get('mode', 'ssh')
    submission = values.get('submission', 'laptop')
    url = str(values.get('url', DEFAULT_URL)).rstrip('/')
    parsed = urllib.parse.urlsplit(url)
    if (mode not in {'ssh', 'rest'} or submission != 'laptop' or
            parsed.scheme not in {'http', 'https'} or not parsed.hostname or
            parsed.username or parsed.password or parsed.query or parsed.fragment or parsed.path or
            any(ord(c) < 32 for c in url)):
        raise ValueError('Choose SSH or REST and enter an HTTP(S) server URL with its port, without credentials or a path.')
    try:
        parsed.port
    except ValueError as exc:
        raise ValueError('Invalid API port.') from exc
    version = str(values.get('version', DEFAULT_VERSION))
    if not re.fullmatch(r'v\d+\.\d+\.\d+', version):
        raise ValueError('API version must look like v0.0.40.')
    with _lock:
        try:
            all_settings = json.loads(CONFIG.read_text())
        except FileNotFoundError:
            all_settings = {}
        all_settings[host] = dict(mode=mode, url=url, version=version, submission=submission)
        CONFIG.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        temporary = CONFIG.with_suffix('.tmp')
        descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(descriptor, 'w') as stream:
            json.dump(all_settings, stream)
        temporary.chmod(0o600); temporary.replace(CONFIG)
        _credentials.pop(host, None); _reads.clear(); _submission_metadata.clear()
    return settings(host)


def enabled(host):
    return settings(host)['mode'] == 'rest'


def credentials(host, renew=False):
    with _lock:
        cached = _credentials.get(host)
        if not renew and cached and cached['expires'] > time.time()+60:
            return cached
        # Never eval shell output or return the token to a browser/log file.
        script = '''import json,os,pwd,subprocess
result=subprocess.run(['scontrol','token','lifespan=3600'],env={k:v for k,v in os.environ.items() if k!='SLURM_JWT'},capture_output=True,text=True)
if result.returncode: raise SystemExit('Could not issue SLURM API token: '+result.stderr.strip())
print(json.dumps(dict(token=result.stdout.strip().removeprefix('SLURM_JWT='),user=pwd.getpwuid(os.getuid()).pw_name,home=os.path.expanduser('~'))))
'''
        try:
            result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes',
                                     '-o', 'ConnectTimeout=10', '--', host, 'python3', '-'],
                                    input=script, text=True, capture_output=True, timeout=25)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise RuntimeError('Could not bootstrap SLURM API authentication over SSH.') from exc
        if result.returncode:
            detail = re.sub(r'SLURM_JWT=\S+|eyJ[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+','[token redacted]',result.stderr.strip())[-1200:]
            raise RuntimeError('Could not bootstrap SLURM API authentication over SSH. Check cluster access and scontrol token support.' + (' '+detail if detail else ''))
        try:
            value = json.loads(result.stdout)
            if not value['token'] or any(c.isspace() for c in value['token']): raise ValueError()
            if not value['home'].startswith('/') or not value['user']: raise ValueError()
        except (ValueError, KeyError, TypeError) as exc:
            raise RuntimeError('Invalid SLURM token bootstrap response.') from exc
        value['expires'] = time.time()+3600
        _credentials[host] = value
        return value


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise RuntimeError('SLURM API redirects are not followed; configure the final API URL.')


def error_details(errors, token=''):
    """Keep SLURM's actual error and code, not just a generic description."""
    if not isinstance(errors, list):
        return ''
    details = []
    reservation_invalid = False
    for entry in errors:
        if not isinstance(entry, dict):
            continue
        messages = []
        for key in ('error', 'description'):
            text = str(entry.get(key) or '').strip()
            if token:
                text = text.replace(token, '[redacted]')
            if text and text not in messages:
                messages.append(text)
        code = entry.get('error_number')
        if code not in (None, 0, '0', ''):
            messages.append('SLURM error ' + str(code))
        if messages:
            details.append('; '.join(messages))
        text = ' '.join(messages).casefold()
        reservation_invalid |= str(code) == '2053' or ('reservation' in text and any(word in text for word in ('invalid', 'expired', 'not found')))
    result = ' | '.join(details)
    if token:
        result = result.replace(token, '[redacted]')
    result = result[:800]
    if reservation_invalid:
        result = result.rstrip('. ') + '. Check the saved SLURM reservation: it may have expired or been removed. Choose a valid reservation, or clear the workflow reservation before dispatching a new job. Resume retains the existing job settings'
    return result


def request(host, endpoint, method='GET', payload=None, timeout=12):
    config = settings(host)
    if not re.fullmatch(r'[a-zA-Z0-9_/-]+', endpoint):
        raise ValueError('Invalid SLURM API endpoint.')
    for attempt in range(2):
        auth = credentials(host, renew=bool(attempt))
        body = json.dumps(payload).encode() if payload is not None else None
        req = urllib.request.Request(config['url']+'/slurm/'+config['version']+'/'+endpoint,
                                     data=body, method=method, headers={
                                         'X-SLURM-USER-TOKEN': auth['token'],
                                         'X-SLURM-USER-NAME': auth['user'],
                                         'Accept': 'application/json', 'Content-Type': 'application/json'})
        try:
            with urllib.request.build_opener(NoRedirect()).open(req, timeout=timeout) as response:
                value = json.load(response)
        except urllib.error.HTTPError as exc:
            if exc.code in {401, 403} and not attempt: continue
            detail = ''
            try:
                detail = error_details(json.load(exc).get('errors', []), auth['token'])
            except (ValueError, AttributeError, TypeError):
                pass
            detail = detail.replace(auth['token'], '[redacted]')
            raise RuntimeError(f'SLURM API returned HTTP {exc.code}: {detail or "request rejected"}. No SSH fallback was used.') from None
        except (urllib.error.URLError, TimeoutError, ValueError) as exc:
            raise RuntimeError('SLURM API request failed or timed out. Check the configured URL and VPN connection. No SSH fallback was used.') from exc
        if not isinstance(value, dict):
            raise RuntimeError('Invalid SLURM API response.')
        if value.get('errors'):
            raise RuntimeError('SLURM API: ' + (error_details(value['errors'], auth['token']) or 'request rejected'))
        if method == 'DELETE':
            failures = [item.get('error', {}) for item in value.get('status', []) if item.get('error', {}).get('code', 0)]
            if failures:
                raise RuntimeError('SLURM cancellation failed: '+ '; '.join(str(e.get('message') or e.get('string') or e.get('code')) for e in failures).replace(auth['token'],'[redacted]'))
        return value


def read(host, endpoint, max_age=5):
    key = (host, settings(host)['url'], settings(host)['version'], endpoint)
    with _lock:
        read_lock = _read_locks.setdefault(key, threading.Lock())
    # Different endpoints must not block each other behind a slow network read.
    with read_lock:
        with _lock:
            cached = _reads.get(key)
            if cached and time.monotonic()-cached[0] < max_age:
                return cached[1]
        value = request(host, endpoint)
        with _lock:
            _reads[key] = (time.monotonic(), value)
        return value


def number(value, default=0):
    if isinstance(value, dict):
        return int(value.get('number', default)) if value.get('set') and not value.get('infinite') else default
    return int(value) if value is not None else default


def partition_nodes(host, partition):
    rows = [n for n in read(host, 'nodes')['nodes'] if partition in n.get('partitions', [])]
    if not rows: raise ValueError('No compute nodes found in the selected partition.')
    result = [dict(sockets=number(n['sockets']), cores_per_socket=number(n['cores']),
                   threads_per_core=number(n['threads'])) for n in rows]
    if any(min(n.values()) < 1 for n in result): raise ValueError('The API returned incomplete node topology.')
    return result


def active_jobs(host, max_age=5):
    active = {'PENDING', 'RUNNING', 'CONFIGURING', 'COMPLETING', 'SUSPENDED', 'RESIZING', 'STAGE_OUT'}
    rows = read(host, 'jobs', max_age=max_age)['jobs']
    elements = {job_id(e): e for j in rows for e in array_elements(j)}
    elements.update({job_id(j): j for j in rows if number(j.get('array_task_id'), -1) >= 0})
    parents = {str(number(j.get('array_job_id'))) for j in elements.values() if number(j.get('array_job_id'))}
    return [j for identifier, j in elements.items() if identifier not in parents and set(j.get('job_state', [])) & active]


def job_id(job):
    array = number(job.get('array_job_id'))
    task = number(job.get('array_task_id'), -1)
    return f'{array}_{task}' if array and task >= 0 else str(job['job_id'])


def elapsed(job):
    start = number(job.get('start_time'))
    return str(max(0, int(time.time())-start)) if start else '0'


def queue_timestamp(value):
    from datetime import datetime, timezone
    try:
        epoch = number(value)
        return datetime.fromtimestamp(epoch, timezone.utc).isoformat() if epoch > 0 else 'Unknown'
    except (ValueError, OverflowError, OSError):
        return 'Unknown'


def queue_text(host, activity=False, fresh=False):
    """Supply a successful REST queue snapshot to the existing file reader."""
    user = credentials(host)['user']
    rows = [j for j in active_jobs(host, max_age=0 if fresh else 5) if j.get('user_name') == user]
    if activity:
        text = '\n'.join(j.get('current_working_directory', '') for j in rows)
    else:
        text = '\n'.join('|'.join([job_id(j), str(number(j.get('array_job_id')) or j['job_id']),
            str(number(j.get('array_task_id'), 0)), '+'.join(j['job_state']), elapsed(j),
            str(max(0, number(j.get('time_limit'))*60-int(elapsed(j)))), j.get('name', ''),
            j.get('current_working_directory', ''), j.get('state_reason', ''),
            queue_timestamp(j.get('submit_time')), queue_timestamp(j.get('start_time'))]).replace('\n', ' ') for j in rows)
    return text


def queue_script(host, activity=False):
    import shlex
    return 'squeue() { printf "%s\\n" '+shlex.quote(queue_text(host, activity))+'; }\n'


def time_minutes(value):
    """SLURM duration syntax, rounded up to the minute used by the REST field."""
    days, separator, rest = str(value).partition('-')
    if separator:
        day_count = int(days); parts = [int(v) for v in rest.split(':')]
        hours, minutes, seconds = (parts+[0,0])[:3]
        total = day_count*86400+hours*3600+minutes*60+seconds
    else:
        parts = [int(v) for v in str(value).split(':')]
        if len(parts)==1:total=parts[0]*60
        elif len(parts)==2:total=parts[0]*60+parts[1]
        elif len(parts)==3:total=parts[0]*3600+parts[1]*60+parts[2]
        else:raise ValueError('Invalid SLURM time format.')
    if total < 0:raise ValueError('Invalid SLURM time format.')
    return (total+59)//60


def batch_request(options, script, cwd, environment):
    """Translate only the sbatch options generated by Slurmy, without dropping any."""
    strings = {'partition':'partition', 'job-name':'name', 'comment':'comment', 'distribution':'distribution',
               'array':'array', 'output':'standard_output', 'reservation':'reservation','error':'standard_error','account':'account','qos':'qos','constraint':'features'}
    integers = {'nodes':'minimum_nodes', 'ntasks':'tasks', 'cpus-per-task':'cpus_per_task',
                'threads-per-core':'threads_per_core', 'ntasks-per-socket':'tasks_per_socket',
                'cores-per-socket':'cores_per_socket', 'sockets-per-node':'sockets_per_node'}
    job = dict(current_working_directory=cwd)
    env = {k:v for k,v in environment.items() if k not in {'SLURM_JWT','SLURMY_API_BRIDGE','BASH_ENV','ENV'}}
    env['SLURM_SUBMIT_DIR'] = cwd
    for option in options:
        if option == 'batch.sh' or option == '--parsable': continue
        if option == '--requeue': job['requeue'] = True; continue
        if option == '--exclusive': job['exclusive'] = ['true']; continue
        name, separator, value = option.removeprefix('--').partition('=')
        if not separator: raise ValueError('Unsupported sbatch option for REST: '+option)
        if name in strings: job[strings[name]] = value
        elif name in integers:
            job[integers[name]] = int(value)
            if name == 'nodes': job['maximum_nodes'] = int(value)
        elif name == 'mem': job['memory_per_node'] = dict(set=True, number=int(value.removesuffix('M')))
        elif name == 'time': job['time_limit'] = dict(infinite=True) if time_minutes(value) == 0 else dict(set=True, number=time_minutes(value))
        elif name == 'open-mode': job['open_mode'] = [value.upper()]
        elif name == 'export':
            for item in value.split(','):
                if item == 'ALL': continue
                key, separator, content = item.partition('=')
                if not separator: raise ValueError('Unsupported environment export.')
                env[key] = content
        elif name == 'signal':
            if value != 'B:TERM@30': raise ValueError('Unsupported warning signal.')
            job.update(kill_warning_signal='TERM', kill_warning_delay=dict(set=True, number=30), kill_warning_flags=['BATCH_JOB'])
        else: raise ValueError('Unsupported sbatch option for REST: '+option)
    if job.get('distribution') == 'block:block' and job.get('minimum_nodes',1) == 1 and job.get('tasks',1) == 1:
        # v0.0.40 accepts this output field but cannot parse it on submission.
        # A single task on one node has no inter-task distribution to specify.
        job.pop('distribution')
    job['environment'] = [key+'='+str(value) for key,value in env.items()]
    return dict(script=script, job=job)


def submission_queue(host):
    """Fresh, compact states plus immutable ownership/path metadata.

    Avoid downloading full resources for every cluster job before every batch.
    Unknown IDs are resolved over REST before they can affect capacity decisions.
    """
    config = settings(host)
    key = (host, config['url'], config['version'])
    states = request(host, 'jobs/state')['jobs']
    with _lock:
        refresh_lock = _read_locks.setdefault(('submission-metadata', *key), threading.Lock())
    with refresh_lock:
        with _lock:
            cached = _submission_metadata.get(key)
            before = dict(cached['jobs']) if cached else {}
            expired = cached is None or time.monotonic()-cached['time'] > 300
        if expired:
            rows = request(host, 'jobs')['jobs']
            jobs = {job_id(j): j for j in rows}
            with _lock:
                # A submission can be remembered while the full GET is in flight.
                # Preserve those additions/replacements when publishing the refresh.
                current = _submission_metadata.get(key)
                if current:
                    jobs.update({identifier: job for identifier, job in current['jobs'].items()
                                 if before.get(identifier) is not job})
                _submission_metadata[key] = dict(time=time.monotonic(), jobs=jobs)
        with _lock:
            jobs = dict(_submission_metadata[key]['jobs'])

    by_numeric = {}
    by_parent = {}
    def index_job(job):
        by_numeric.setdefault(str(job['job_id']), job)
        parent = number(job.get('array_job_id'))
        if parent:
            by_parent.setdefault(str(parent), job)
    for job in jobs.values():
        index_job(job)

    def lookup(row):
        identifier = job_id(row)
        return (jobs.get(identifier) or by_numeric.get(str(row['job_id']))
                or jobs.get(identifier.split('_')[0]) or by_parent.get(identifier.split('_')[0]))

    active = {'PENDING','RUNNING','CONFIGURING','COMPLETING','SUSPENDED','RESIZING','STAGE_OUT'}
    normalized = {}
    explicit = {}
    for row in states:
        identifier = str(row['job_id'])
        if not re.fullmatch(r'[0-9]+(?:_[0-9]+)?', identifier):
            # SLURM retains cancelled array summaries such as 1136022_*.
            # They consume no slots; unknown active shapes still fail closed.
            if not set(row.get('state', row.get('job_state', []))) & active:
                continue
            raise RuntimeError('Unrecognized SLURM state identifier; capacity is unknown.')
        parent, separator, task = identifier.partition('_')
        if separator:
            row = {**row, 'array_job_id': int(parent), 'array_task_id': int(task)}
        for element in array_elements(row):
            normalized[job_id(element)] = element
        if number(row.get('array_task_id'), -1) >= 0:
            explicit[job_id(row)] = row
    # Valid terminal elements override grouped pending rows in either order.
    normalized.update(explicit)
    parents = {str(number(r.get('array_job_id'))) for r in normalized.values()
               if number(r.get('array_job_id'))}
    result = {}
    for identifier, row in normalized.items():
        if identifier in parents: continue  # Parent placeholder is not an extra task.
        state = row.get('state', row.get('job_state', []))
        if not set(state) & active: continue
        job = lookup(row)
        if job is None:
            for resolved in request(host, 'job/'+identifier).get('jobs', []):
                jobs[job_id(resolved)] = resolved
                index_job(resolved)
                with _lock:
                    current = _submission_metadata.get(key)
                    if current is not None:
                        current['jobs'][job_id(resolved)] = resolved
            job = lookup(row)
        if job is None:
            raise RuntimeError('Could not resolve an active SLURM task; submission capacity is unknown.')
        element = {**job, 'job_state': state}
        if number(row.get('array_job_id')):
            element.update(array_job_id=row['array_job_id'], array_task_id=row.get('array_task_id'))
        result[job_id(element)] = element
    return list(result.values())


def remember_submission(host, identifier, payload):
    config = settings(host)
    key = (host, config['url'], config['version'])
    with _lock:
        if key not in _submission_metadata: return
    fields = payload['job']
    job = dict(job_id=int(identifier), user_name=credentials(host)['user'],
               current_working_directory=fields['current_working_directory'], comment=fields.get('comment',''),
               account=fields.get('account', ''), name=fields.get('name', ''))
    if fields.get('array'):
        job.update(array_job_id=int(identifier), array_task_string=fields['array'])
    elements = {job_id(element): element for element in array_elements(job)}
    with _lock:
        cached = _submission_metadata.get(key)
        if cached is not None:
            cached['jobs'].update(elements)


def array_elements(job):
    """A pending array descriptor can represent several submitted SLURM tasks."""
    specification = job.get('array_task_string') or ''
    if not specification or number(job.get('array_task_id'), -1) >= 0 or not number(job.get('array_job_id')):
        return [job]
    elements = {}
    for part in specification.split('%', 1)[0].split(','):
        match = re.fullmatch(r'(\d+)(?:-(\d+)(?::(\d+))?)?', part)
        if not match: raise RuntimeError('Cannot interpret the pending SLURM array indices.')
        first, last, step = match.groups()
        first, last, step = int(first), int(last or first), int(step or 1)
        if step < 1 or last < first or (last-first)//step > 100000:
            raise RuntimeError('Invalid or excessive SLURM array range.')
        for index in range(first, last+1, step):
            elements[index] = {**job, 'array_task_id':dict(set=True, number=index)}
    return list(elements.values())
