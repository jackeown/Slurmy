#!/usr/bin/env python3
"""Coordinate one user's Slurmy submissions on a shared Slurm login filesystem.

This runs on the cluster head node. The lock covers the queue snapshot and
sbatch itself, so simultaneous Slurmy submitters cannot both claim one slot.
"""
from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


JOB_ID = re.compile(r"[A-Za-z0-9._-]+\Z")
LIMIT_ERRORS = (
    "AssocMaxSubmitJobLimit", "AssocGrpSubmitJobsLimit",
    "QOSMaxSubmitJobPerUserLimit", "QOSMaxSubmitJobPerAccountLimit",
    "QOSGrpSubmitJobLimit", "QOSGrpSubmitJobsLimit",
    "job submit limit", "maximum number of submitted jobs",
)
LEASE_SECONDS = 45
OBSERVATION_SECONDS = 300


def command(argv: list[str], timeout: int = 20) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, text=True, capture_output=True, timeout=timeout, check=False)


def save_json(path: Path, value: dict) -> None:
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def read_json(path: Path) -> dict:
    try:
        result = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    return result if isinstance(result, dict) else {}


def reported_limit(user: str) -> int | None:
    try:
        result = command(["sacctmgr", "-nP", "show", "assoc", f"user={user}",
                          "format=User,Account,Partition,MaxSubmitJobs"], timeout=10)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode:
        return None
    limits = []
    for line in result.stdout.splitlines():
        fields = line.split("|")
        if fields and fields[0] == user and fields[-1].isdigit() and int(fields[-1]) > 0:
            limits.append(int(fields[-1]))
    return min(limits) if limits else None


def queue_snapshot(user: str, jobs_root: Path) -> tuple[int, int, dict[str, int]]:
    result = command(["squeue", "-h", "-r", "-u", user, "-o", "%i|%Z"])
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or "squeue failed")
    total = other = 0
    own: dict[str, int] = {}
    prefix = str(jobs_root) + os.sep
    for line in result.stdout.splitlines():
        if not line.strip():
            continue
        total += 1
        directory = line.split("|", 1)[1] if "|" in line else ""
        name = directory[len(prefix):] if directory.startswith(prefix) else ""
        if name and JOB_ID.fullmatch(name):
            own[name] = own.get(name, 0) + 1
        else:
            other += 1
    return total, other, own


def fair_targets(demands: dict[str, int], capacity: int, turn: int = 0) -> dict[str, int]:
    """Max-min fair allocation, redistributing unused shares."""
    targets = {name: 0 for name in demands}
    remaining = max(0, capacity)
    waiting = sorted(demands, key=lambda name: (demands[name], name))
    while waiting:
        level, extra = divmod(remaining, len(waiting))
        smallest = demands[waiting[0]]
        if smallest > level:
            ordered = sorted(waiting)
            rotated = ordered[turn % len(ordered):] + ordered[:turn % len(ordered)]
            bonus = set(rotated[:extra])
            for name in ordered:
                targets[name] = min(demands[name], level + (name in bonus))
            break
        name = waiting.pop(0)
        targets[name] = smallest
        remaining -= smallest
    return targets


def main() -> int:
    if len(sys.argv) < 3 or sys.argv[1] not in {"attempt", "release"}:
        raise SystemExit("usage: submit_coordinator.py attempt|release JOB_ID [MAX_OUTSTANDING -- sbatch options]")
    action, job_id = sys.argv[1:3]
    if not JOB_ID.fullmatch(job_id):
        raise SystemExit("Invalid Slurmy job ID")
    if action == "attempt":
        if len(sys.argv) < 6 or not sys.argv[3].isdigit() or int(sys.argv[3]) < 1 or sys.argv[4] != "--":
            raise SystemExit("attempt requires a positive maximum and sbatch arguments")
        requested = int(sys.argv[3])
        sbatch_args = sys.argv[5:]
    else:
        requested, sbatch_args = 0, []
    base = Path.home() / "slurmy"
    jobs_root = base / "jobs"
    leases_dir = base / "submitters"
    base.mkdir(mode=0o700, exist_ok=True)
    leases_dir.mkdir(mode=0o700, exist_ok=True)
    with (base / "submit.lock").open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        lease = leases_dir / f"{job_id}.json"
        if action == "release":
            lease.unlink(missing_ok=True)
            return 0
        now = time.time()
        save_json(lease, {"job_id": job_id, "requested": requested, "updated": now})
        demands = {}
        for path in leases_dir.glob("*.json"):
            record = read_json(path)
            if now - float(record.get("updated", 0)) > LEASE_SECONDS or not JOB_ID.fullmatch(str(record.get("job_id", ""))):
                path.unlink(missing_ok=True)
                continue
            demand = int(record.get("requested", 0))
            if demand > 0:
                demands[record["job_id"]] = demand
        user = os.environ.get("USER") or command(["id", "-un"]).stdout.strip()
        total, other, own = queue_snapshot(user, jobs_root)
        capacity_file = base / "submit-capacity.json"
        capacity_record = read_json(capacity_file)
        if now - float(capacity_record.get("checked", 0)) > OBSERVATION_SECONDS:
            capacity_record["reported"] = reported_limit(user)
            capacity_record["checked"] = now
            save_json(capacity_file, capacity_record)
        observed = capacity_record.get("observed") if now - float(capacity_record.get("observed_at", 0)) < OBSERVATION_SECONDS else None
        known = [value for value in (capacity_record.get("reported"), observed) if isinstance(value, int) and value > 0]
        ceiling = min(known) if known else None
        active_without_submitter = sum(count for name, count in own.items() if name not in demands)
        available = (ceiling - other - active_without_submitter) if ceiling is not None else sum(demands.values())
        target = fair_targets(demands, available, int(capacity_record.get("turn", 0))).get(job_id, requested)
        current = own.get(job_id, 0)
        if current >= target or (ceiling is not None and total >= ceiling):
            print(f"WAIT|{current} active for this job; fair share {target}; {total} user jobs; "
                  f"{'reported/observed limit ' + str(ceiling) if ceiling is not None else 'limit not reported'}")
            return 0
        try:
            submitted = command(["sbatch", *sbatch_args], timeout=60)
        except (OSError, subprocess.TimeoutExpired) as exc:
            print(f"ERROR|Could not confirm sbatch acceptance: {exc}. Inspect Slurm before retrying.")
            return 2
        message = (submitted.stdout + "\n" + submitted.stderr).strip().replace("\n", " ")
        if submitted.returncode == 0:
            slurm_id = submitted.stdout.strip().split(";", 1)[0]
            if not slurm_id.isdigit():
                print(f"ERROR|sbatch returned an unrecognized ID: {message}")
                return 2
            capacity_record["turn"] = int(capacity_record.get("turn", 0)) + 1
            save_json(capacity_file, capacity_record)
            print(f"ACCEPTED|{slurm_id}|{current + 1}/{target} fair share; {total + 1} user jobs"
                  + (f"; submit limit {ceiling}" if ceiling is not None else ""))
            return 0
        if any(token.casefold() in message.casefold() for token in LIMIT_ERRORS):
            if total == 0:
                print(f"ERROR|Slurm rejected submission with no queued jobs: {message}")
                return 2
            prior = capacity_record.get("observed")
            capacity_record.update(observed=min(total, prior) if isinstance(prior, int) and prior > 0 else total,
                                   observed_at=now)
            save_json(capacity_file, capacity_record)
            print(f"WAIT|Slurm rejected submission at {total} user jobs; "
                  f"observed ceiling {capacity_record['observed']} (expires in five minutes): {message}")
            return 0
        print(f"ERROR|sbatch rejected submission: {message}")
        return 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"ERROR|Submission coordination failed: {exc}")
        raise SystemExit(2)
