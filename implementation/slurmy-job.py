#!/usr/bin/env python3
"""Stop or resume one Slurmy job on the configured cluster."""
from ssh_transport import connection_options

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stop', 'resume', 'delete'))
    parser.add_argument('job_id')
    parser.add_argument('--host', default=os.environ.get('SLURMY_HOST', 'datalab'))
    parser.add_argument('--delete-local', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.job_id):
        parser.error('Invalid Slurmy job ID.')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.host):
        parser.error('Invalid SSH host alias.')
    templates = Path(__file__).with_name('templates')
    local_paths = [Path(__file__).resolve().parent.parent / 'job-results' / args.job_id,
                   Path(__file__).resolve().parent.parent / 'job-results' / 'verbose' / args.job_id]
    if args.action == 'delete' and args.delete_local and any(p.is_symlink() for p in local_paths):
        print('Local result directory is a symlink; job was kept.', file=sys.stderr)
        return 1
    # Always finish stopping before resume or deletion, including lingering
    # allocations from an earlier Stop request or a concurrent submission.
    actions = ['stop'] + ([args.action] if args.action != 'stop' else [])
    for action in actions:
        print({'stop': 'Stopping submission and cancelling running and queued SLURM tasks; completed results are kept…',
               'resume': 'Resuming unfinished solver-problem pairs; completed results are kept…',
               'delete': 'All tasks stopped. Deleting cluster job files…'}[action], flush=True)
        try:
            with (templates / f'remote_{action}.sh').open('rb') as stream:
                result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--',
                                         args.host, 'bash', '-s', '--', args.job_id], stdin=stream, check=False)
        except OSError as error:
            print(f'Could not {action} job over SSH: {error}', file=sys.stderr)
            return 1
        if result.returncode:
            return result.returncode
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
