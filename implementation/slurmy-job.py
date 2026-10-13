#!/usr/bin/env python3
"""Stop or resume one Slurmy job on the configured cluster."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('stop', 'resume'))
    parser.add_argument('job_id')
    parser.add_argument('--host', default=os.environ.get('SLURMY_HOST', 'datalab'))
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.job_id):
        parser.error('Invalid Slurmy job ID.')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.host):
        parser.error('Invalid SSH host alias.')
    script = Path(__file__).with_name('templates') / f'remote_{args.action}.sh'
    try:
        with script.open('rb') as stream:
            result = subprocess.run(['ssh', '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--',
                                     args.host, 'bash', '-s', '--', args.job_id], stdin=stream, check=False)
    except OSError as error:
        print(f'Could not {args.action} job over SSH: {error}', file=sys.stderr)
        return 1
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
