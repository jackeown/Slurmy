#!/usr/bin/env python3
"""Stop or resume one Slurmy job on the configured cluster."""
from ssh_transport import connection_options

import argparse
import base64
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import shutil
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stop', 'resume', 'delete'))
    parser.add_argument('job_id')
    parser.add_argument('--host', default=os.environ.get('SLURMY_HOST', 'datalab'))
    parser.add_argument('--delete-local', action='store_true')
    parser.add_argument('--if-idle', action='store_true', help='resume only if no controller or SLURM tasks remain')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.job_id):
        parser.error('Invalid Slurmy job ID.')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.host):
        parser.error('Invalid SSH host alias.')
    if args.if_idle and args.action != 'resume':
        parser.error('--if-idle is only valid with resume')
    templates = Path(__file__).with_name('templates')
    local_paths = [Path(__file__).resolve().parent.parent / 'job-results' / args.job_id,
                   Path(__file__).resolve().parent.parent / 'job-results' / 'verbose' / args.job_id,
                   Path(__file__).resolve().parent.parent / 'job-results' / 'starexec' / args.job_id]
    if args.action == 'delete' and args.delete_local and any(p.is_symlink() for p in local_paths):
        print('Local result directory is a symlink; job was kept.', file=sys.stderr)
        return 1
    # Always finish stopping before resume or deletion, including lingering
    # allocations from an earlier Stop request or a concurrent submission.
    actions = ['resume'] if args.if_idle else ['stop'] + ([args.action] if args.action != 'stop' else [])
    import local_dispatch
    if not args.if_idle:
        local_dispatch.stop(args.host, args.job_id)
    elif local_dispatch.is_active(args.host, args.job_id):
        print('Local submission is already active; keeping it.', flush=True)
        return 0
    for action in actions:
        print({'stop': 'Stopping submission and cancelling running and queued SLURM tasks; completed results are kept…',
               'resume': 'Resuming unfinished solver-problem pairs; completed results are kept…',
               'delete': 'All tasks stopped. Deleting cluster job files…'}[action], flush=True)
        try:
            import slurm_api
            with (tempfile.TemporaryFile() if action == 'resume' else (templates / f'remote_{action}.sh').open('rb')) as stream:
                if action == 'resume':
                    if args.if_idle:
                        stream.write((templates / 'remote_recover.sh').read_bytes())
                        stream.write(b'\n')
                    updates = {name: base64.b64encode(path.read_bytes()).decode('ascii') for name, path in (
                        ('submit_coordinator.py', templates.with_name('submit_coordinator.py')),
                        ('remote_submit.sh', templates/'remote_submit.sh'), ('call.sh', templates/'call.sh'),
                        ('batch.sh', templates/'batch.sh'))}
                    from szs import shell_library
                    updates['szs.sh'] = base64.b64encode(shell_library().encode()).decode('ascii')
                    stream.write(('set -euo pipefail\n'
                                  '[[ "$1" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2\n'
                                  '[[ -d "$HOME/slurmy/jobs/$1" && ! -L "$HOME/slurmy/jobs/$1" ]] || exit 2\n'
                                  'python3 - "$HOME/slurmy/jobs/$1" <<\'SLURMY_HELPER\'\n'
                                  'import base64,shutil,sys\nfrom pathlib import Path\n'
                                  'root=Path(sys.argv[1])\n'
                                  f'for name, encoded in {updates!r}.items():\n'
                                  ' p=root/name; backup=root/(name+".before-managed-controller")\n'
                                  ' if p.is_file() and not backup.exists(): shutil.copy2(p,backup)\n'
                                  ' t=p.with_suffix(".tmp"); t.write_bytes(base64.b64decode(encoded)); t.replace(p)\n'
                                  'SLURMY_HELPER\n').encode())
                    stream.write((templates / f'remote_{action}.sh').read_bytes())
                    stream.seek(0)
                attempts = 3 if action in {'stop', 'delete'} else 1
                for attempt in range(1, attempts + 1):
                    stream.seek(0)
                    try:
                        budget = [] if action == 'resume' else ['timeout', '--signal=TERM', '--kill-after=5', '120']
                        if slurm_api.enabled(args.host):
                            result = local_dispatch.remote_action(args.host, stream.read().decode(), [args.job_id], emit=action!='resume')
                        else:
                            result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--',
                                                     args.host, *budget, 'env', '-u', 'BASH_ENV', '-u', 'ENV', 'bash', '--noprofile', '--norc', '-s', '--', args.job_id], stdin=stream, check=False, timeout=None if action == 'resume' else 180,
                                                     stdout=subprocess.PIPE if action == 'resume' else None,
                                                     text=True)
                        code = result.returncode
                        if action == 'resume':
                            resume_args = None
                            for line in (result.stdout or '').splitlines():
                                if line.startswith('SLURMY_RESUME_ARGS '):
                                    resume_args = json.loads(line[len('SLURMY_RESUME_ARGS '):])
                                else:
                                    print(line, flush=True)
                            if code == 0 and resume_args:
                                import local_dispatch
                                code = local_dispatch.run(args.host, resume_args)
                    except subprocess.TimeoutExpired:
                        code = 255
                    if code == 0:
                        break
                    if code not in ({255, 124, 137, 75} if action == 'delete' else {255, 124, 137}) or attempt == attempts:
                        if code in {255, 124, 137}:
                            message = {
                                'stop': 'Stop could not be confirmed. Reconnect and retry; completed results are kept.',
                                'delete': 'Deletion could not be confirmed. It may already have completed; reconnect and retry to confirm.',
                                'resume': 'Resume could not be confirmed. The job may already be running; check its refreshed status before retrying Resume.',
                            }[action]
                            print(message, file=sys.stderr, flush=True)
                        elif code == 75:
                            print('Deletion is still in progress remotely. Retry later to confirm completion.', file=sys.stderr, flush=True)
                        return code
                    reason = 'Connection interrupted' if code != 75 else 'Deletion still in progress'
                    print(f'{reason}; retrying {action} ({attempt + 1}/{attempts})…', flush=True)
                    time.sleep(attempt)
        except (OSError, RuntimeError) as error:
            print(f'Could not {action} job: {error}', file=sys.stderr)
            return 1
    if args.action == 'delete':
        if args.delete_local:
            print('Deleting downloaded results…', flush=True)
            for directory in local_paths:
                if directory.is_dir():
                    shutil.rmtree(directory)
        print('Job deleted.', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
