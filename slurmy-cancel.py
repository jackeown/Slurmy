#!/usr/bin/env python
"""Select and cancel an active Slurm job over SSH."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import os
import re
import shutil
import subprocess
import sys
from typing import Sequence


VERSION = "0.1.0"
DEFAULT_HOST = os.environ.get("SLURMY_HOST", "datalab")
SLURM_ID_RE = re.compile(r"^[0-9]+(?:_[0-9]+|\+[0-9]+|\.[A-Za-z0-9]+)?$")


REMOTE_LIST = r"""
set -euo pipefail
command -v squeue >/dev/null || { echo "squeue is not available" >&2; exit 1; }
squeue --noheader --array --user "$USER" --format='%F|%T|%j|%R'
"""


REMOTE_CANCEL = r"""
set -euo pipefail
job_id=$1
command -v scancel >/dev/null || { echo "scancel is not available" >&2; exit 1; }
scancel "$job_id"
"""


class CancelError(RuntimeError):
    """A job could not be listed or cancelled."""


@dataclass
class ActiveJob:
    job_id: str
    name: str
    states: set[str] = field(default_factory=set)
    elements: int = 0
    location: str = ""


def run_remote(
    host: str,
    script: str,
    *arguments: str,
    timeout: float,
) -> subprocess.CompletedProcess[str]:
    if shutil.which("ssh") is None:
        raise CancelError("ssh is not installed or not on PATH")
    command = [
        "ssh",
        "-T",
        "-o",
        "BatchMode=yes",
        "-o",
        f"ConnectTimeout={max(1, int(timeout))}",
        "--",
        host,
        "bash",
        "-s",
        "--",
        *arguments,
    ]
    try:
        process = subprocess.run(
            command,
            input=script,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout + 20,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise CancelError(f"SSH command timed out after {timeout + 20:g}s") from exc
    if process.returncode != 0:
        message = process.stderr.strip()
        raise CancelError(message or f"ssh exited with status {process.returncode}")
    return process


def active_jobs(host: str, timeout: float) -> list[ActiveJob]:
    process = run_remote(host, REMOTE_LIST, timeout=timeout)
    jobs: dict[str, ActiveJob] = {}
    for line in process.stdout.splitlines():
        fields = line.split("|", 3)
        if len(fields) != 4:
            continue
        job_id, state, name, location = (field.strip() for field in fields)
        if not job_id.isdigit():
            continue
        job = jobs.setdefault(job_id, ActiveJob(job_id=job_id, name=name))
        job.states.add(state or "UNKNOWN")
        job.elements += 1
        if location and not job.location:
            job.location = location
    return sorted(jobs.values(), key=lambda job: int(job.job_id), reverse=True)


def choose_job(jobs: Sequence[ActiveJob], host: str) -> str | None:
    if not jobs:
        print(f"No active Slurm jobs were found on {host}.")
        return None

    print(f"Active Slurm jobs on {host}:\n")
    for index, job in enumerate(jobs, start=1):
        state = ", ".join(sorted(job.states))
        elements = f" · {job.elements} array elements" if job.elements > 1 else ""
        location = f" · {job.location}" if job.location else ""
        print(f"  {index:>2}. {job.job_id:<10} {state:<12} {job.name}{elements}{location}")

    while True:
        try:
            answer = input("\nSelect a job to cancel by number or job ID (q to quit): ").strip()
        except EOFError as exc:
            raise CancelError("no job was selected") from exc
        if answer.lower() in {"q", "quit"}:
            return None
        if answer.isdigit():
            choice = int(answer)
            if 1 <= choice <= len(jobs):
                return jobs[choice - 1].job_id
            if any(job.job_id == answer for job in jobs):
                return answer
        print("Enter a listed number or job ID, or q to quit.")


def cancel_job(host: str, job_id: str, timeout: float) -> None:
    if not SLURM_ID_RE.fullmatch(job_id):
        raise CancelError(
            "invalid Slurm job ID; use a base ID such as 12345 or an element such as 12345_7"
        )
    run_remote(host, REMOTE_CANCEL, job_id, timeout=timeout)
    print(f"Cancellation requested for Slurm job {job_id} on {host}.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="slurmy-cancel.py",
        description="Select an active Slurm job or cancel a specified job over SSH.",
    )
    parser.add_argument(
        "job_id",
        nargs="?",
        help="Slurm job ID to cancel immediately; omit to choose from active jobs",
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help="SSH host alias (default: $SLURMY_HOST, or datalab)",
    )
    parser.add_argument(
        "--ssh-timeout",
        type=float,
        default=10.0,
        metavar="SECONDS",
        help="SSH connection timeout (default: 10)",
    )
    parser.add_argument("--version", action="version", version=f"Slurmy cancel {VERSION}")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.ssh_timeout <= 0:
        raise SystemExit("--ssh-timeout must be greater than zero")
    try:
        job_id = args.job_id
        if job_id is None:
            job_id = choose_job(active_jobs(args.host, args.ssh_timeout), args.host)
        if job_id is not None:
            cancel_job(args.host, job_id, args.ssh_timeout)
    except CancelError as exc:
        print(f"Slurmy cancel: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
