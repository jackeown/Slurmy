#!/usr/bin/env python3
"""Drive finite submission waves locally; accepted SLURM jobs run independently."""
from __future__ import annotations
import json
import fcntl
import hashlib
import signal
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time

import slurm_api
from ssh_transport import connection_options

PREFIX = 'SLURMY_SCHEDULER_RPC '


def rpc(host, message):
    argv = message['argv']
    if argv[0] == 'squeue':
        user = slurm_api.credentials(host)['user']
        jobs = [j for j in slurm_api.submission_queue(host) if j.get('user_name') == user]
        if '-A' in argv:
            account = argv[argv.index('-A')+1]
            jobs = [j for j in jobs if (j.get('account') or 'default') == account]
        format_string = argv[argv.index('-o')+1] if '-o' in argv else '%i|%Z|%k'
        formats = {'%i|%Z|%k':lambda j:'|'.join([slurm_api.job_id(j),j.get('current_working_directory',''),j.get('comment') or '']),
                   '%i|%Z':lambda j:'|'.join([slurm_api.job_id(j),j.get('current_working_directory','')]),
                   '%Z':lambda j:j.get('current_working_directory','')}
        if format_string not in formats: raise ValueError('Unsupported REST queue format.')
        stdout = '\n'.join(formats[format_string](j) for j in jobs)+'\n'
        return dict(returncode=0, stdout=stdout, stderr='')
    if argv[0] == 'scancel':
        for identifier in argv[1:]:
            import re
            if not re.fullmatch(r'[0-9]+(?:_[0-9]+)?',identifier): raise ValueError('Invalid SLURM task ID.')
            slurm_api.request(host,'job/'+identifier,'DELETE')
        slurm_api._reads.clear()
        return dict(returncode=0,stdout='',stderr='')
    if argv[0] != 'sbatch':
        raise ValueError('Unsupported scheduler bridge command.')
    payload = slurm_api.batch_request(argv[1:], message['script'], message['cwd'], message['environment'])
    try:
        result = slurm_api.request(host, 'job/submit', 'POST', payload)
    except RuntimeError as error:
        job = payload['job']
        settings = ', '.join(f"{key}={str(job.get(key) or '(none)')!r}"
                             for key in ('partition', 'account', 'reservation'))
        raise RuntimeError(f'{error} Requested SLURM settings: {settings}.') from None
    identifier = result.get('job_id') or (result.get('result') or {}).get('job_id')
    if not str(identifier).isdigit(): raise RuntimeError('API submission did not return a job ID.')
    slurm_api.remember_submission(host, identifier, payload)
    slurm_api._reads.clear()
    return dict(returncode=0, stdout=str(identifier)+'\n', stderr='')


def remote_action(host, script, args, emit=False):
    """Run file operations over SSH, but service live scheduler checks over REST."""
    import base64
    helper = """import json,os,sys
with os.fdopen(os.dup(3),'w') as stream:
 stream.write('SLURMY_SCHEDULER_RPC '+json.dumps(dict(argv=sys.argv[1:]))+'\\n');stream.flush()
with os.fdopen(os.dup(4),'r') as stream: result=json.loads(stream.readline())
sys.stdout.write(result['stdout']);sys.stderr.write(result['stderr']);sys.exit(result['returncode'])
"""
    encoded = base64.b64encode(helper.encode()).decode()
    prefix = "exec 3>&1 4<&0;\n" + "function slurmy_rpc() { python3 -c "+shlex.quote("import base64;exec(base64.b64decode("+repr(encoded)+"))")+" \"$@\"; };\n" + "squeue() { slurmy_rpc squeue \"$@\"; }; scancel() { slurmy_rpc scancel \"$@\"; };\n"
    command = shlex.join(['timeout','--signal=TERM','--kill-after=5','150','env','-u','BASH_ENV','-u','ENV','bash','--noprofile','--norc','-c',prefix+script,'slurmy-action',*args])
    process = subprocess.Popen(['ssh',*connection_options(),'-T','-o','BatchMode=yes','-o','ConnectTimeout=10','--',host,command],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    lines=[]
    try:
        for line in process.stdout:
            if line.startswith(PREFIX):
                try: response=rpc(host,json.loads(line[len(PREFIX):]))
                except (RuntimeError,ValueError,OSError) as error: response=dict(returncode=1,stdout='',stderr=str(error))
                process.stdin.write(json.dumps(response)+'\n');process.stdin.flush()
            else:
                lines.append(line)
                if emit:print(line,end='',flush=True)
        return subprocess.CompletedProcess(command,process.wait(),''.join(lines))
    finally:
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        process.stdin.close();process.stdout.close()


def lock_path(host, job_id):
    return slurm_api.CONFIG.parent/'submission-locks'/hashlib.sha256((host+':'+job_id).encode()).hexdigest()


def is_active(host, job_id):
    path = lock_path(host, job_id)
    if not path.is_file(): return False
    with path.open('r') as stream:
        try: fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError: return True
    return False


def stop(host, job_id):
    if not is_active(host, job_id): return
    try: pid = int(lock_path(host, job_id).read_text())
    except (OSError, ValueError): raise RuntimeError('Cannot identify the active local submitter.')
    if pid == os.getpid(): raise RuntimeError('Cannot stop this submission from within itself.')
    try: os.kill(pid, signal.SIGTERM)
    except ProcessLookupError: pass
    deadline = time.monotonic()+10
    while is_active(host, job_id):
        if time.monotonic() > deadline: raise RuntimeError('Local submitter has not stopped yet.')
        time.sleep(.1)


def run(host, args, script=None, poll_seconds=15):
    path = lock_path(host, args[0]); path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    with path.open('a+') as lock:
        try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError: raise RuntimeError('This job already has a local submitter.')
        lock.seek(0); lock.truncate(); lock.write(str(os.getpid())); lock.flush()
        previous = signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
        try:
            return _run(host, args, script, poll_seconds)
        finally:
            signal.signal(signal.SIGTERM, previous)


def _run(host, args, script=None, poll_seconds=15):
    script = script or (Path(__file__).parent/'templates/remote_dispatch.sh').read_text()
    rest = slurm_api.enabled(host)
    print('Submission phase: Laptop-driven submission; keep this local server running until every batch is accepted.', flush=True)
    following = list(args)
    while True:
        prefix = 'exec 3>&1 4<&0; export SLURMY_API_BRIDGE=1;\n' if rest else ''
        remote = shlex.join(['timeout','--signal=TERM','--kill-after=5','120','env','-u','BASH_ENV','-u','ENV','bash','--noprofile','--norc','-c',prefix+script,'slurmy-wave',*following])
        process = subprocess.Popen(['ssh', *connection_options(), '-T', '-o','BatchMode=yes','-o','ConnectTimeout=10','--',host,remote],
                                   stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        try:
            for line in process.stdout:
                if rest and line.startswith(PREFIX):
                    try: response = rpc(host, json.loads(line[len(PREFIX):]))
                    except (RuntimeError, ValueError, OSError) as exc:
                        response = dict(returncode=1, stdout='', stderr=str(exc))
                    process.stdin.write(json.dumps(response)+'\n'); process.stdin.flush()
                else:
                    print(line, end='', flush=True)
            code = process.wait()
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill();process.wait()
            process.stdin.close(); process.stdout.close()
        if code == 0:
            print('Submission phase: All batches accepted. This computer may now disconnect; SLURM runs the submitted tasks.', flush=True)
            return 0
        if code != 75:
            print('Submission stopped before all batches were accepted. Submitted tasks continue; use Resume after reconnecting.', flush=True)
            return code
        # Continue the persisted resume worklist; never recompute it each wave.
        while len(following) < 6: following.append('0')
        following[5] = '2'
        print(f'Submission waiting locally: checking again in {poll_seconds}s; already accepted tasks continue on the cluster.', flush=True)
        while True:
            time.sleep(poll_seconds)
            if not rest: break
            # Wait with HTTP only; do not reopen SSH while the queue is full.
            auth = slurm_api.credentials(host)
            try:
                jobs = slurm_api.submission_queue(host)
            except RuntimeError as exc:
                print('Submission waiting locally: '+str(exc), flush=True)
                continue
            account = args[8] if len(args) > 8 and args[8] != '-' else 'default'
            own = [j for j in jobs if j.get('user_name') == auth['user'] and (j.get('account') or 'default') == account]
            directory = auth['home']+'/slurmy/jobs/'+args[0]
            active = sum(j.get('current_working_directory') == directory for j in own)
            limit = slurm_api.account_limit(host, account, args[1])
            if (limit is None or len(own) < limit) and active < int(args[2]): break
            print('Submission waiting locally: submission window is still full; no SSH session is open.', flush=True)


if __name__ == '__main__':
    host, *args = sys.argv[1:]
    script = (Path(__file__).parent/'remote_dispatch.sh').read_text()
    raise SystemExit(run(host, args, script))
