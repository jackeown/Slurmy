#!/usr/bin/env python3
"""Download Slurmy results by job name or ID, using the web app's download engine.

    from download import download
    success = download('My workflow (1)', mode='default')

The function is silent and returns a boolean. Rich is needed only for the
interactive command-line picker. Default destinations match the job button.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

from implementation.ssh_transport import connection_options

ROOT = Path(__file__).resolve().parent
MODES = ('default', 'verbose', 'starexec')

# One SSH request reads only immutable identity metadata, never result archives.
# An ID embeds the original dispatch time, unlike directory/metadata mtimes,
# which can change during downloads, progress, resume, or rename operations.
REMOTE_JOBS = r'''
import json, math, re
from pathlib import Path
root=Path.home()/'slurmy/jobs'
jobs=[]
if root.is_dir():
    for directory in root.iterdir():
        identifier=directory.name
        if directory.is_symlink() or not directory.is_dir() or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',identifier):
            continue
        metadata={}
        path=directory/'metadata.json'
        try:
            if not path.is_symlink():
                metadata=json.loads(path.read_text(encoding='utf-8'))
            if not isinstance(metadata,dict):metadata={}
        except (OSError,ValueError):metadata={}
        match=re.search(r'_(\d{9,})_\d+$',identifier) or re.search(r'_(\d{9,})$',identifier)
        if match:submitted=float(match[1])
        else:
            try:submitted=float(metadata.get('submitted_epoch') or metadata.get('created_epoch') or 0)
            except (ValueError,TypeError):submitted=0
        if not math.isfinite(submitted) or submitted<0:submitted=0
        name=metadata.get('job_name')
        if not isinstance(name,str) or not name.strip():
            workflow=metadata.get('workflow_directory')
            name=Path(workflow).name if isinstance(workflow,str) and workflow else identifier[:match.start()] if match else identifier
        jobs.append(dict(id=identifier,name=name.strip() or identifier,submitted=submitted))
print(json.dumps(jobs,ensure_ascii=True))
'''


class DownloadError(RuntimeError):
    """An actionable download or selection error."""


@dataclass(frozen=True)
class Job:
    id: str
    name: str
    submitted: float


def _settings(host, ssh_timeout):
    host = host if host is not None else os.environ.get('SLURMY_HOST', 'datalab')
    if not isinstance(host,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*',host):
        raise ValueError('Use an SSH host alias containing letters, numbers, dots, underscores or hyphens.')
    timeout=float(ssh_timeout)
    if not math.isfinite(timeout) or timeout<=0:
        raise ValueError('SSH timeout must be a positive finite number.')
    return host,timeout


def list_jobs(*, host=None, ssh_timeout=10) -> list[Job]:
    """Read available jobs in descending original-dispatch order."""
    host,timeout=_settings(host,ssh_timeout)
    result=subprocess.run(['ssh',*connection_options(),'-T','-o','BatchMode=yes',
        '-o',f'ConnectTimeout={max(1,int(timeout))}','--',host,
        shlex.join(['env','-u','BASH_ENV','-u','ENV','python3','-'])],
        input=REMOTE_JOBS,text=True,capture_output=True,timeout=timeout+10,check=False)
    if result.returncode:
        raise DownloadError(result.stderr.strip() or 'Could not read the cluster job list.')
    try:
        items=json.loads(result.stdout)
        if not isinstance(items,list):raise ValueError('Expected a job list.')
        jobs=[]
        for item in items:
            identifier,name,submitted=item['id'],item['name'],float(item['submitted'])
            if (not isinstance(identifier,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',identifier)
                    or not isinstance(name,str) or not math.isfinite(submitted) or submitted<0):
                raise ValueError('Invalid job metadata.')
            jobs.append(Job(identifier,name,submitted))
        if len({job.id for job in jobs})!=len(jobs):raise ValueError('Duplicate job IDs.')
    except (ValueError,TypeError,KeyError,OverflowError) as exc:
        raise DownloadError('The cluster returned an invalid job list.') from exc
    return sorted(jobs,key=lambda job:(job.submitted,job.id),reverse=True)


def _resolve(jobs, job_name, latest):
    if latest:
        if job_name is not None:raise ValueError('Choose a job name or latest, not both.')
        if not jobs:raise DownloadError('No Slurmy jobs are available on this host.')
        return jobs[0]
    if not isinstance(job_name,str) or not job_name.strip() or any(ord(c)<32 for c in job_name):
        raise ValueError('Specify a job name or ID, or select latest.')
    name=job_name.strip()
    matches=[job for job in jobs if job.id==name]
    if not matches:matches=[job for job in jobs if job.name==name]
    if len(matches)>1:
        raise DownloadError(f'Job name {name!r} is ambiguous. Use one of these IDs: '+', '.join(job.id for job in matches))
    if not matches:raise DownloadError(f'No job named {name!r} exists on this host.')
    return matches[0]


def _perform(job_name, mode, *, host=None, output=None, latest=False, ssh_timeout=10,
             show_output=False, selected_job=None):
    if mode not in MODES:raise ValueError('Download mode must be default, verbose, or starexec.')
    host,timeout=_settings(host,ssh_timeout)
    if selected_job is None:
        # Reject invalid selectors before opening SSH.
        if latest and job_name is not None:raise ValueError('Choose a job name or latest, not both.')
        if not latest and (not isinstance(job_name,str) or not job_name.strip() or any(ord(c)<32 for c in job_name)):
            raise ValueError('Specify a job name or ID, or select latest.')
        selected_job=_resolve(list_jobs(host=host,ssh_timeout=timeout),job_name,latest)
    argv=[sys.executable,str(ROOT/'implementation/slurmy-sync.py'),selected_job.id,
          '--host',host,'--ssh-timeout',str(timeout)]
    if mode!='default':argv.append('--'+mode)
    if output is not None:
        argv.extend(['--output',str(Path(output).expanduser().absolute())])
    # Redirection silences Python and native child output without changing
    # the caller's global stdout/stderr or buffering verbose transfer logs.
    result=subprocess.run(argv,cwd=ROOT,stdin=subprocess.DEVNULL,text=True,
                          stdout=None if show_output else subprocess.DEVNULL,
                          stderr=None if show_output else subprocess.DEVNULL,check=False)
    if result.returncode:
        raise DownloadError(f'Results download failed (exit {result.returncode}).')
    return True


def download(job_name: str | None = None, mode: str = 'default', *, host=None,
             output=None, latest=False, ssh_timeout=10) -> bool:
    """Silently download a job; return True on success and False on failure.

    ``job_name`` is an exact display name or stable Slurmy ID. Use
    ``download(latest=True)`` to select the most recently dispatched job.
    ``mode`` is default, verbose, or starexec. Optional output paths are relative
    to the caller; the default is this repository's job-results directory, just
    like the web app. Errors and cancellation return False without printing.
    """
    try:
        return _perform(job_name,mode,host=host,output=output,latest=latest,ssh_timeout=ssh_timeout)
    except (OSError,ValueError,TypeError,RuntimeError,subprocess.SubprocessError,KeyboardInterrupt):
        return False


def _pick(jobs, default_mode='default'):
    try:
        from rich.console import Console
        from rich.prompt import IntPrompt
        from rich.table import Table
        from rich.text import Text
    except ImportError as exc:
        raise DownloadError('Interactive mode needs Rich. Install it with: python -m pip install rich') from exc
    if not jobs:raise DownloadError('No Slurmy jobs are available on this host.')
    console=Console()
    table=Table(title='Download Mode')
    table.add_column('#',justify='right');table.add_column('Mode');table.add_column('Files')
    for index,(mode,description) in enumerate(zip(MODES,[
        'Summary CSV + stdout, stderr and postprocessing archives',
        'Complete results, logs, diagnostics and metadata',
        'StarExec-compatible information and output ZIPs']),1):
        table.add_row(str(index),mode,description)
    console.print(table)
    mode=MODES[IntPrompt.ask('Download mode',choices=['1','2','3'],default=MODES.index(default_mode)+1,console=console)-1]
    table=Table(title='Jobs · Most Recently Dispatched First')
    table.add_column('#',justify='right');table.add_column('Job Name');table.add_column('Dispatched');table.add_column('Job ID')
    for index,job in enumerate(jobs,1):
        try:timestamp=datetime.fromtimestamp(job.submitted).astimezone().strftime('%Y-%m-%d %H:%M:%S') if job.submitted else 'Unknown'
        except (OverflowError,OSError,ValueError):timestamp='Unknown'
        table.add_row(str(index),Text(job.name),timestamp,Text(job.id))
    console.print(table)
    selected=IntPrompt.ask('Job',choices=[str(i) for i in range(1,len(jobs)+1)],default=1,console=console)
    return jobs[selected-1],mode


def build_parser():
    parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('job_name',nargs='?',help='exact displayed job name or stable job ID')
    selection=parser.add_mutually_exclusive_group()
    selection.add_argument('--latest',action='store_true',help='download the most recently dispatched job')
    selection.add_argument('--interactive',action='store_true',help='pick the mode and job from Rich lists')
    parser.add_argument('--mode',choices=MODES,default='default',help='download format (default: default)')
    parser.add_argument('--host',default=os.environ.get('SLURMY_HOST','datalab'),help='SSH alias (default: $SLURMY_HOST or datalab)')
    parser.add_argument('--output',type=Path,help='destination directory (relative paths use the current directory)')
    parser.add_argument('--ssh-timeout',type=float,default=10,help='SSH connection timeout in seconds (default: 10)')
    return parser


def main(argv=None):
    parser=build_parser();args=parser.parse_args(argv)
    if args.job_name is not None and (args.latest or args.interactive):
        parser.error('Choose a job name, --latest, or --interactive, not a combination.')
    if args.job_name is None and not (args.latest or args.interactive):
        parser.error('Specify a job name, --latest, or --interactive.')
    try:_settings(args.host,args.ssh_timeout)
    except (ValueError,TypeError) as exc:parser.error(str(exc))
    try:
        selected=None
        if args.interactive:
            selected,args.mode=_pick(list_jobs(host=args.host,ssh_timeout=args.ssh_timeout),args.mode)
        _perform(args.job_name,args.mode,host=args.host,output=args.output,latest=args.latest,
                 ssh_timeout=args.ssh_timeout,show_output=True,selected_job=selected)
        return 0
    except (KeyboardInterrupt, EOFError):
        print('\nDownload cancelled; existing local files were kept.',file=sys.stderr)
        return 130
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:
        print(f'Download: {exc}',file=sys.stderr)
        return 1


if __name__=='__main__':
    raise SystemExit(main())
