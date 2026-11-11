"""Run finite submission waves as SLURM jobs, independent of the local app.

Each controller reserves its successor before submitting worker batches. On a
full window it makes that successor depend on a worker, then releases its own
core. In particular, it never holds a node while waiting for exclusive workers.
"""
from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import time

ROLE = 'slurmy-controller:v1'
NAME = 'slurmy:controller'


def command(args, timeout=60):
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or f'{args[0]} failed')
    return result.stdout.strip()


def queue():
    text = command(['squeue', '-h', '-r', '-u', os.environ['USER'], '-o', '%i|%Z|%k'])
    return [line.split('|', 2) for line in text.splitlines() if len(line.split('|', 2)) == 3]


def submit(job, args, dependency=None):
    wrap = shlex.join(['env', '-u', 'BASH_ENV', '-u', 'ENV', 'python3',
                       str(job/'submission_controller.py'), 'run', *args])
    options = ['sbatch', '--parsable', '--partition='+args[1], '--job-name='+NAME,
               '--comment='+ROLE, '--chdir='+str(job), '--nodes=1', '--ntasks=1',
               '--cpus-per-task=1', '--threads-per-core=1', '--mem=256M',
               '--time=00:10:00', '--open-mode=append', '--output=submission-controller.log',
               '--export=ALL', '--wrap='+wrap]
    if len(args) > 6 and args[6] not in ('', '-'):
        options.append('--reservation='+args[6])
    if dependency:
        options.append('--dependency=afterany:'+dependency)
    # Share the same account-wide submission lock used by worker submissions.
    with (Path.home()/'slurmy/submit.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        identifier = command(options).split(';', 1)[0]
    if not identifier.isdigit():
        raise RuntimeError('Unrecognized SLURM controller ID: '+identifier)
    return identifier


def save_identifier(job, identifier):
    temporary = job/'.submission-controller.id.tmp'
    temporary.write_text(identifier+'\n')
    temporary.replace(job/'.submission-controller.id')


def launch(job, args):
    active = [row for row in queue() if row[1] == str(job) and row[2] == ROLE]
    if active:
        print(f'Slurmy job ID: {args[0]}', flush=True)
        print('Submission controller is already queued or running in SLURM; keeping it.', flush=True)
        return
    identifier = submit(job, args)
    save_identifier(job, identifier)
    print(f'Slurmy job ID: {args[0]}', flush=True)
    print(f'Submission phase: SLURM controller {identifier} queued; submission continues without this computer', flush=True)


def run(job, args):
    if (job/'.stopped').exists():
        return
    current = os.environ.get('SLURM_JOB_ID', '')
    if not current.isdigit():
        raise RuntimeError('Submission controller must run inside a SLURM allocation.')
    following = list(args)
    while len(following) < 6:
        following.append('0')
    following[5] = '2'  # Continue the recorded resume worklist, never rebuild it.
    successor = None
    handed_off = False
    try:
        # Reserve the continuation's queue slot before filling the worker window.
        successor = submit(job, following, current)
        environment = {**os.environ, 'SLURMY_MANAGED_CONTROLLER': '1',
                       'SLURMY_CONTROLLER_DEADLINE': str(int(time.time())+480)}
        result = subprocess.run(['env', '-u', 'BASH_ENV', '-u', 'ENV', 'bash',
                                 '--noprofile', '--norc', './remote_submit.sh', *args],
                                cwd=job, env=environment)
        if (job/'.stopped').exists():
            return
        if result.returncode != 75:
            if result.returncode:
                raise RuntimeError(f'Submission wave failed (exit {result.returncode}).')
            return
        # Wait for an already queued worker to free a slot. Include this
        # controller in the dependency so the successor cannot run too early.
        rows = queue()
        workers = [row[0] for row in rows if row[2] != ROLE and row[0].replace('_', '').isdigit()]
        own = [row[0] for row in rows if row[1] == str(job) and row[2] != ROLE
               and row[0].replace('_', '').isdigit()]
        if not workers:
            try:
                capacity = json.loads((Path.home()/'slurmy/submit-capacity.json').read_text())
            except (OSError, ValueError):
                capacity = {}
            observed = capacity.get('observed') if time.time()-capacity.get('observed_at', 0) < 300 else None
            limits = [value for value in (capacity.get('reported'), observed)
                      if isinstance(value, int) and value > 0]
            if limits and len(rows) >= min(limits):
                raise RuntimeError('SLURM submission capacity is occupied by controllers; '
                                   'at least three account submission slots are needed for one job. '
                                   'Wait for other jobs to finish, then Resume.')
        dependency = current + (':'+(own or workers)[0] if own or workers else '')
        if not workers:
            # A read-only queue outage or a brief scheduler transition: retry
            # later without retaining an allocation or spinning on the head.
            options = ['scontrol', 'update', 'JobId='+successor,
                       'Dependency=afterany:'+dependency, 'StartTime=now+60seconds']
        else:
            options = ['scontrol', 'update', 'JobId='+successor, 'Dependency=afterany:'+dependency]
        command(options)
        save_identifier(job, successor)
        handed_off = True
        print(f'Submission waiting: continuation controller {successor} queued; releasing controller core', flush=True)
    finally:
        if successor and not handed_off:
            command(['scancel', successor])


def main():
    action, *args = sys.argv[1:]
    if action not in ('launch', 'run') or len(args) < 5:
        raise ValueError('Expected launch|run JOB PARTITION WINDOW PREFIX NAME [RESUME RESERVATION AUTO_NAME].')
    import re
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args[0]):
        raise ValueError('Invalid Slurmy job ID.')
    job = Path.home()/'slurmy/jobs'/args[0]
    if not job.is_dir() or job.is_symlink():
        raise ValueError('Committed Slurmy job directory is missing.')
    if action == 'launch':
        launch(job, args)
    else:
        def interrupted(signum, frame):
            raise InterruptedError('SLURM controller interrupted')
        signal.signal(signal.SIGTERM, interrupted)
        run(job, args)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print('Submission controller error: '+str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
