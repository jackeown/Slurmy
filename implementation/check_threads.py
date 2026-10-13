#!/usr/bin/env python3
"""Check requested hardware threads against a Slurm partition before preparing."""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys


def check_requested(host: str, partition: str, maximum: int) -> int:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", partition):
        raise ValueError("Invalid Slurm partition name.")
    try:
        process = subprocess.run(
            ["ssh", "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", host,
             "sinfo", "-p", partition, "-N", "-h", "-o", "%Z"],
            capture_output=True, text=True, timeout=20, check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValueError(f"Cannot check threads per core on {host}/{partition}: {error}") from error
    if process.returncode:
        raise ValueError(f"Cannot check threads per core on {host}/{partition}: "
                         f"{process.stderr.strip() or 'SSH or sinfo failed'}")
    values = set(process.stdout.split())
    if not values or any(not value.isdecimal() or int(value) < 1 for value in values):
        raise ValueError(f"Cannot determine threads per core for partition {partition} on {host}.")
    if len(values) != 1:
        raise ValueError(f"Partition {partition} has mixed threads-per-core topology; "
                         "choose a homogeneous partition.")
    supported = int(values.pop())
    if maximum > supported:
        raise ValueError(f"Workflow requests {maximum} threads per core, but partition "
                         f"{partition} on {host} supports {supported}. Edit the solver configuration.")
    return supported


def check(host: str, partition: str, jobpairs: str) -> None:
    maximum = 1
    with open(jobpairs, encoding="utf-8-sig", newline="") as stream:
        rows = (map(json.loads, stream) if jobpairs.endswith(".jsonl")
                else csv.DictReader(stream))
        for number, row in enumerate(rows, 1 if jobpairs.endswith(".jsonl") else 2):
            try:
                requested = int(row.get("threads_per_core") or "1")
            except ValueError as error:
                raise ValueError(f"{jobpairs} row {number}: threads_per_core must be a positive integer") from error
            if requested < 1:
                raise ValueError(f"{jobpairs} row {number}: threads_per_core must be a positive integer")
            maximum = max(maximum, requested)
    supported = check_requested(host, partition, maximum)
    print(f"✅ Threads per core: requested up to {maximum}; {partition} supports {supported}.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jobpairs")
    parser.add_argument("--host", required=True)
    parser.add_argument("--partition", required=True)
    arguments = parser.parse_args()
    try:
        check(arguments.host, arguments.partition, arguments.jobpairs)
    except (OSError, subprocess.TimeoutExpired, ValueError) as error:
        print(f"Slurm topology preflight: {error}", file=sys.stderr)
        sys.exit(1)
