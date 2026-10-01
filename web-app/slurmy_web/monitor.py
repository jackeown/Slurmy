#!/usr/bin/env python
"""Read-only cluster monitoring shared by the local web interface."""

from __future__ import annotations

import base64
import csv
from dataclasses import dataclass, field
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess
import time
from typing import Any



VERSION = "0.2.0"
DEFAULT_HOST = os.environ.get("SLURMY_HOST", "datalab")
NORMAL_TASK_RESULTS = {"ok", "time-limit", "memory-limit", "counter-satisfiable"}
SLURM_ERROR_STATES = {
    "BOOT_FAIL",
    "DEADLINE",
    "FAILED",
    "NODE_FAIL",
    "OUT_OF_MEMORY",
    "REVOKED",
    "TIMEOUT",
}
SLURM_INCOMPLETE_STATES = SLURM_ERROR_STATES | {"CANCELLED", "PREEMPTED"}


def slurm_state_code(state: str) -> str:
    return state.upper().split()[0].rstrip("+") if state else ""

# This script is sent to `ssh HOST bash -s`. It reads only the small monitoring
# files under $HOME/slurmy/jobs and emits NUL-delimited records. Python is not needed on
# the cluster. Large solver inputs and result archives are never transferred.
REMOTE_SNAPSHOT_SCRIPT = r"""
set -u

DETAIL=${1:-}
BASE="$HOME/slurmy/jobs"

printf 'REMOTE_HOME\0%s\0' "$BASE"
if [[ ! -d "$BASE" ]]; then
    printf 'DETAIL\0\0'
    printf 'SQUEUE\0\0SACCT\0\0'
    exit 0
fi

shopt -s nullglob
job_dirs=("$BASE"/*)
if [[ -z "$DETAIL" ]]; then
    for directory in "${job_dirs[@]}"; do
        [[ -d "$directory" ]] || continue
        candidate=${directory##*/}
        if [[ -z "$DETAIL" || "$candidate" > "$DETAIL" ]]; then
            DETAIL=$candidate
        fi
    done
fi
printf 'DETAIL\0%s\0' "$DETAIL"

oldest=
for directory in "${job_dirs[@]}"; do
    [[ -d "$directory" ]] || continue
    job=${directory##*/}
    created=$(stat -c %Y "$directory" 2>/dev/null || printf '0')
    printf 'JOB\0%s\0%s\0' "$job" "$created"

    if [[ -f "$directory/metadata.json" ]]; then
        printf 'METADATA\0%s\0' "$job"
        cat "$directory/metadata.json"
        printf '\0'
    fi

    csv_results=("$directory"/results/batch_*.csv)
    # Keep TSV and pre-batch JSONL jobs readable in the historical dashboard.
    tsv_results=("$directory"/results/batch_*.tsv "$directory"/results/chunk_*.tsv)
    if (( ${#csv_results[@]} )); then
        printf 'SUMMARY\0%s\0' "$job"
        # Slurmy quotes every CSV field. The summary uses only numeric and enum
        # columns, which cannot themselves contain commas.
        summary_payload=$(awk -F, '
            function value(field) {
                if (field ~ /^".*"$/) {
                    sub(/^"/, "", field); sub(/"$/, "", field)
                    gsub(/""/, "\"", field)
                }
                return field
            }
            NF >= 3 && value($1) ~ /^[0-9]+$/ {
                id = value($1)
                complete[id] = value($2)
                result_status[id] = value($3)
                if (result_status[id] == "solver-error" && value($4) == "143" && value($16) == "")
                    suspect_archive[id] = value($15)
                if (result_status[id] == "solver-error" && value($16) == "Timeout")
                    result_status[id] = "time-limit"
                if (result_status[id] == "solver-error" && value($16) == "CounterSatisfiable")
                    result_status[id] = "counter-satisfiable"
                if (result_status[id] == "solver-error" && value($16) != "" && value($4) + 0 < 128 &&
                    tolower(value($16)) !~ /^(error|oserror|inputerror|syntaxerror|semanticerror|typeerror|usageerror)$/ &&
                    tolower(value($16)) != "timeout" && tolower(value($16)) != "countersatisfiable")
                    result_status[id] = "ok"
                wall[id] = value($5)
            }
            END {
                for (id in result_status) {
                    if (complete[id] == "true") {
                        completed++
                        count[result_status[id]]++
                        if (wall[id] ~ /^[0-9]+([.][0-9]+)?$/) {
                            wall_sum += wall[id]
                            if (wall[id] > wall_max) wall_max = wall[id]
                        }
                    } else {
                        unresolved[result_status[id]]++
                    }
                }
                printf "completed\t%d\nwall_sum\t%.9f\nwall_max\t%.9f\n", completed, wall_sum, wall_max
                for (status in count) printf "status\t%s\t%d\n", status, count[status]
                for (status in unresolved) printf "unresolved\t%s\t%d\n", status, unresolved[status]
                for (id in suspect_archive) printf "candidate\t%s\t%s\n", id, suspect_archive[id]
            }
        ' "${csv_results[@]}")
        printf '%s\n' "$summary_payload"
        printf '\0'
    elif (( ${#tsv_results[@]} )); then
        printf 'SUMMARY\0%s\0' "$job"
        awk -F '\t' '
            NF >= 3 && $1 ~ /^[0-9]+$/ {
                complete[$1] = $2
                result_status[$1] = $3
                wall[$1] = $5
            }
            END {
                for (id in result_status) {
                    if (complete[id] == "true") {
                        completed++
                        count[result_status[id]]++
                        if (wall[id] ~ /^[0-9]+([.][0-9]+)?$/) {
                            wall_sum += wall[id]
                            if (wall[id] > wall_max) wall_max = wall[id]
                        }
                    } else {
                        unresolved[result_status[id]]++
                    }
                }
                printf "completed\t%d\nwall_sum\t%.9f\nwall_max\t%.9f\n", completed, wall_sum, wall_max
                for (status in count) printf "status\t%s\t%d\n", status, count[status]
                for (status in unresolved) printf "unresolved\t%s\t%d\n", status, unresolved[status]
            }
        ' "${tsv_results[@]}"
        printf '\0'
    else
        legacy_results=("$directory"/results/chunk_*.jsonl)
        if (( ${#legacy_results[@]} )); then
            printf 'LEGACY_RESULTS\0%s\0' "$job"
            cat "${legacy_results[@]}"
            printf '\0'
        fi
    fi

    for progress_file in "$directory"/progress/task_*.csv; do
        printf 'CALL_PROGRESS\0%s\0' "$job"
        cat "$progress_file"
        printf '\0'
    done
    csv_progress=("$directory"/progress/batch_*.csv)
    tsv_progress=("$directory"/progress/batch_*.tsv "$directory"/progress/chunk_*.tsv)
    if (( ${#csv_progress[@]} )); then
        printf 'PROGRESS_CSV\0%s\0' "$job"
        for progress_file in "${csv_progress[@]}"; do
            printf '"%s",' "${progress_file##*/}"
            cat "$progress_file"
        done
        printf '\0'
    elif (( ${#tsv_progress[@]} )); then
        printf 'PROGRESS_TSV\0%s\0' "$job"
        for progress_file in "${tsv_progress[@]}"; do
            printf '%s\t' "${progress_file##*/}"
            cat "$progress_file"
        done
        printf '\0'
    fi

    if [[ -f "$directory/submission.csv" ]]; then
        printf 'SUBMISSION_CSV\0%s\0' "$job"
        cat "$directory/submission.csv"
        printf '\0'
    elif [[ -f "$directory/submission.tsv" ]]; then
        printf 'SUBMISSION_TSV\0%s\0' "$job"
        cat "$directory/submission.tsv"
        printf '\0'
    fi
    if [[ -f "$directory/submission.state" ]]; then
        printf 'SUBMISSION_STATE\0%s\0' "$job"
        cat "$directory/submission.state"
        printf '\0'
    fi

    # Older runsolver result rows may say solver-error/143 and MEMOUT=false even
    # though their archived watcher explicitly records a memory-limit kill.
    # Inspect only those exceptional rows; never infer a limit from exit 143.
    if (( ${#csv_results[@]} )); then
        while IFS=$'\t' read -r kind task_id archive; do
            [[ "$kind" == candidate ]] || continue
            [[ "$task_id" =~ ^[0-9]+$ && "$archive" =~ ^[A-Za-z0-9._-]+[.]tar[.]gz$ ]] || continue
            archive_path="$directory/results/$archive"
            [[ -f "$archive_path" ]] || continue
            if tar -xOzf "$archive_path" --wildcards '*watcher.log' 2>/dev/null | grep -Fq 'Maximum memory exceeded:'; then
                printf 'RESULT_OVERRIDE\0%s\0%s\0memory-limit\0' "$job" "$task_id"
            fi
        done <<< "$summary_payload"
    fi

    if [[ "$created" =~ ^[0-9]+$ ]] && { [[ -z "$oldest" ]] || (( created < oldest )); }; then
        oldest=$created
    fi

    if [[ "$job" == "$DETAIL" ]]; then
        if (( ${#csv_results[@]} )); then
            printf 'RESULTS_CSV\0%s\0' "$job"
            cat "${csv_results[@]}"
            printf '\0'
        elif (( ${#tsv_results[@]} )); then
            printf 'RESULTS_TSV\0%s\0' "$job"
            cat "${tsv_results[@]}"
            printf '\0'
        fi
        legacy_results=("$directory"/results/chunk_*.jsonl)
        if (( ${#legacy_results[@]} )); then
            printf 'RESULTS_JSONL\0%s\0' "$job"
            cat "${legacy_results[@]}"
            printf '\0'
        fi
        if [[ -f "$directory/manifest.jsonl" ]]; then
            printf 'MANIFEST\0%s\0' "$job"
            cat "$directory/manifest.jsonl"
            printf '\0'
        fi

        batch_files=("$directory"/batches/batch_*.sh "$directory"/chunks/chunk_*.sh)
        for batch_file in "${batch_files[@]}"; do
            (
                set +u
                source "$batch_file"
                if [[ "$batch_file" =~ (batch|chunk)_([0-9]+)[.]sh$ ]] && declare -p TASK_IDS >/dev/null 2>&1; then
                    batch_id=$((10#${BASH_REMATCH[2]}))
                    for task_id in "${TASK_IDS[@]}"; do
                        printf 'BATCH_TASK\0%s\0%s\0%s\0' "$job" "$batch_id" "$task_id"
                    done
                fi
                declare -p TASK_IDS TASK_KEYS TASK_PROBLEM_RELS TASK_COMMANDS TASK_SOLVER_ROOT_RELS >/dev/null 2>&1 || exit 0
                for index in "${!TASK_IDS[@]}"; do
                    solver=
                    if declare -p TASK_SYSTEMS >/dev/null 2>&1; then
                        solver=${TASK_SYSTEMS[$index]}
                    fi
                    printf 'TASKDEF\0%s\0%s\0%s\0%s\0%s\0%s\0%s\0' \
                        "$job" "${TASK_IDS[$index]}" "${TASK_KEYS[$index]}" \
                        "$solver" "${TASK_PROBLEM_RELS[$index]}" "${TASK_COMMANDS[$index]}" \
                        "${TASK_SOLVER_ROOT_RELS[$index]}"
                done
            )
        done

        log_count=0
        log_files=("$directory"/logs/*.out)
        for log_file in "${log_files[@]}"; do
            (( log_count >= 100 )) && break
            printf 'LOG\0%s\0%s\0' "$job" "${log_file##*/}"
            tail -n 160 "$log_file" 2>/dev/null || true
            printf '\0'
            log_count=$((log_count + 1))
        done
    fi
done

printf 'SQUEUE\0'
squeue -r -h -u "$USER" -o '%i|%F|%K|%T|%M|%L|%j|%Z|%R' 2>/dev/null \
    | awk -F '|' -v prefix="$BASE/" 'index($8, prefix) == 1'
printf '\0'

if [[ -n "$oldest" ]]; then
    start=$(date -d "@$((oldest - 86400))" +%F 2>/dev/null || printf '1970-01-01')
else
    start=$(date +%F)
fi
printf 'SACCT\0'
sacct -X -S "$start" -n -P \
    -o JobIDRaw,JobID,JobName,State,Elapsed,Start,End,Submit,WorkDir,NodeList \
    2>/dev/null | awk -F '|' -v prefix="$BASE/" 'index($9, prefix) == 1'
printf '\0'
"""

# Task output is extracted only when the user asks for it. To keep a mistaken huge
# output from flooding SSH or the terminal, each displayed member is capped at
# 1 MiB, preserving both its beginning and end.
REMOTE_TASK_OUTPUT_SCRIPT = r"""
set -u

JOB=$1
ARCHIVE=$2
TASK_ID=$3
TASK_KEY=$4
LIMIT=1048576

if ! command -v tar >/dev/null || ! command -v base64 >/dev/null; then
    printf 'ERROR\0The cluster needs tar and base64 to display task output.\0'
    exit 0
fi

if [[ ! "$JOB" =~ ^[A-Za-z0-9._-]+$ ]] \
    || [[ ! "$ARCHIVE" =~ ^[A-Za-z0-9._-]+[.]tar[.]gz$ ]] \
    || [[ ! "$TASK_ID" =~ ^[0-9]+$ ]] \
    || [[ ! "$TASK_KEY" =~ ^[A-Fa-f0-9]+$ ]]; then
    printf 'ERROR\0Invalid task-output request.\0'
    exit 0
fi

archive_path="$HOME/slurmy/jobs/$JOB/results/$ARCHIVE"
if [[ ! -f "$archive_path" ]]; then
    printf 'ERROR\0Result archive not found: %s\0' "$ARCHIVE"
    exit 0
fi

stem=$(printf 'task_%09d_%s' "$TASK_ID" "$TASK_KEY")
emit_member() {
    local kind=$1 suffix=$2 member="$stem.$2" size truncated half
    if ! tar -tzf "$archive_path" "$member" >/dev/null 2>&1; then
        member="./$member"
        if ! tar -tzf "$archive_path" "$member" >/dev/null 2>&1; then
            printf 'TASK_OUTPUT\0%s\0-1\0false\0\0' "$kind"
            return
        fi
    fi
    size=$(tar -tvzf "$archive_path" "$member" 2>/dev/null | awk 'NR == 1 {print $3}')
    [[ "$size" =~ ^[0-9]+$ ]] || size=0
    truncated=false
    printf 'TASK_OUTPUT\0%s\0%s\0' "$kind" "$size"
    if (( size > LIMIT )); then
        truncated=true
    fi
    printf '%s\0' "$truncated"
    if [[ "$truncated" == true ]]; then
        half=$((LIMIT / 2))
        {
            tar -xOzf "$archive_path" "$member" 2>/dev/null | head -c "$half" || true
            printf '\n\n--- middle omitted by Slurmy monitor ---\n\n'
            tar -xOzf "$archive_path" "$member" 2>/dev/null | tail -c "$half" || true
        } | base64 -w0
    else
        tar -xOzf "$archive_path" "$member" 2>/dev/null | base64 -w0
    fi
    printf '\0'
}

emit_member solver solver.log
emit_member solver_stderr solver-stderr.log
emit_member limiter_stdout limiter-stdout.log
emit_member controller controller.log
emit_member watcher watcher.log
emit_member variables var
"""


@dataclass
class ResultRecord:
    task_id: int
    complete: bool
    status: str
    return_code: str = ""
    wall_seconds: float | None = None
    cpu_seconds: float | None = None
    max_memory_kib: float | None = None
    task_key: str = ""
    archive: str = ""
    szs_status: str = ""
    problem: str = ""
    command: str = ""


@dataclass
class TaskDefinition:
    task_id: int
    task_key: str = ""
    solver: str = ""
    problem: str = ""
    command: str = ""
    limiter_command: str = ""
    solver_root: str = ""


@dataclass
class BatchProgress:
    batch_id: int
    state: str
    task_id: int | None
    started_epoch: float | None
    updated_epoch: float | None
    finished_count: int = 0


@dataclass
class SlurmRecord:
    display_id: str
    array_job_id: str
    array_task_id: str
    state: str
    elapsed: str
    time_left: str
    job_name: str
    workdir: str
    location: str
    source: str
    start: str = ""
    end: str = ""
    submitted: str = ""


@dataclass
class OutputStream:
    name: str
    content: str
    size: int
    truncated: bool
    exists: bool


@dataclass
class TaskOutput:
    job_id: str
    task_id: int
    archive: str
    streams: dict[str, OutputStream]


@dataclass
class JobSnapshot:
    job_id: str
    remote_dir: str
    created_epoch: float
    metadata: dict[str, Any] = field(default_factory=dict)
    completed: int = 0
    wall_sum: float = 0.0
    wall_max: float = 0.0
    status_counts: dict[str, int] = field(default_factory=dict)
    unresolved_counts: dict[str, int] = field(default_factory=dict)
    results: dict[int, ResultRecord] = field(default_factory=dict)
    tasks: dict[int, TaskDefinition] = field(default_factory=dict)
    progress: dict[int, BatchProgress] = field(default_factory=dict)
    call_progress: dict[int, BatchProgress] = field(default_factory=dict)
    task_batches: dict[int, int] = field(default_factory=dict)
    submission_offsets: dict[str, int] = field(default_factory=dict)
    submitted_batches: set[int] = field(default_factory=set)
    submission_state: str = ""
    submission_count: int = 0
    slurm: list[SlurmRecord] = field(default_factory=list)
    logs: dict[str, str] = field(default_factory=dict)

    @property
    def task_count(self) -> int:
        return int(self.metadata.get("task_count", 0) or 0)

    @property
    def batch_count(self) -> int:
        # The fallback keeps jobs generated before the batch terminology change visible.
        return int(self.metadata.get("batch_count", self.metadata.get("chunk_count", 0)) or 0)

    @property
    def batch_size(self) -> int:
        return int(self.metadata.get("batch_size", self.metadata.get("chunk_size", 1)) or 1)

    @property
    def job_name(self) -> str:
        name = self.metadata.get("job_name")
        if name:
            return str(name)
        for record in self.slurm:
            if record.job_name:
                return record.job_name
        return "Slurmy"

    @property
    def issue_count(self) -> int:
        task_issues = sum(
            count
            for status, count in self.status_counts.items()
            if status.lower() not in NORMAL_TASK_RESULTS
        ) + sum(
            count
            for status, count in self.unresolved_counts.items()
            if status.lower() not in NORMAL_TASK_RESULTS
        )
        unresolved_slurm_failure = (
            self.completed < self.task_count
            and not self.active_records
            and any(
                slurm_state_code(record.state) in SLURM_ERROR_STATES
                for record in self.slurm
            )
        )
        setup_incomplete = not self.metadata and not self.submission_offsets and not self.slurm
        submission_incomplete = (self.submission_state == 'failed' or
                                 (self.submission_state != 'submitting' and
                                  bool(self.submitted_batches) and
                                  self.batch_count > len(self.submitted_batches) and
                                  not self.active_records))
        return task_issues + int(unresolved_slurm_failure) + int(setup_incomplete) + int(submission_incomplete)

    @property
    def percent(self) -> float:
        return 100.0 * self.effective_completed / self.task_count if self.task_count else 0.0

    @property
    def active_batch_ids(self) -> set[int]:
        batches: set[int] = set()
        for record in self.active_records:
            if record.state.upper() not in {"RUNNING", "COMPLETING"}:
                continue
            if record.array_task_id.isdigit():
                batches.add(
                    self.submission_offsets.get(record.array_job_id, 0)
                    + int(record.array_task_id)
                )
        return batches

    @property
    def effective_completed(self) -> int:
        live = sum(
            progress.finished_count
            for batch_id, progress in self.progress.items()
            if batch_id in self.active_batch_ids
        )
        return min(self.task_count, self.completed + live)

    @property
    def active_records(self) -> list[SlurmRecord]:
        return [record for record in self.slurm if record.source == "queue"]

    @property
    def state(self) -> str:
        if not self.metadata and not self.submission_offsets and not self.slurm:
            return "SETUP INCOMPLETE"
        active_states = {record.state.upper() for record in self.active_records}
        if "RUNNING" in active_states or "COMPLETING" in active_states:
            return "RUNNING"
        if active_states:
            return "PENDING"
        if self.task_count and self.completed >= self.task_count:
            return "DONE" if not self.issue_count else "DONE · ERRORS"
        if self.submission_state == 'submitting':
            return 'SUBMITTING'
        historical_states = {slurm_state_code(record.state) for record in self.slurm}
        if historical_states & SLURM_INCOMPLETE_STATES:
            return "INCOMPLETE"
        if self.completed:
            return "INCOMPLETE"
        return "SUBMITTED" if self.submission_offsets or self.slurm else "UNKNOWN"


@dataclass
class ClusterSnapshot:
    host: str
    remote_home: str
    detail_job: str
    fetched_epoch: float
    jobs: list[JobSnapshot]

    def find_job(self, job_id: str | None) -> JobSnapshot | None:
        return next((job for job in self.jobs if job.job_id == job_id), None)


PROTOCOL_ARITY = {
    "CALL_PROGRESS": 2,
    "BATCH_TASK": 3,
    "REMOTE_HOME": 1,
    "DETAIL": 1,
    "JOB": 2,
    "METADATA": 2,
    "SUMMARY": 2,
    "LEGACY_RESULTS": 2,
    "PROGRESS_CSV": 2,
    "PROGRESS_TSV": 2,
    "SUBMISSION_CSV": 2,
    "SUBMISSION_STATE": 2,
    "RESULT_OVERRIDE": 3,
    "SUBMISSION_TSV": 2,
    "RESULTS_CSV": 2,
    "RESULTS_TSV": 2,
    "RESULTS_JSONL": 2,
    "MANIFEST": 2,
    "TASKDEF": 7,
    "LOG": 3,
    "SQUEUE": 1,
    "SACCT": 1,
}


def decode_protocol(data: bytes) -> list[tuple[str, list[str]]]:
    parts = data.split(b"\0")
    records: list[tuple[str, list[str]]] = []
    index = 0
    while index < len(parts) and parts[index]:
        kind = parts[index].decode("utf-8", "replace")
        index += 1
        if kind not in PROTOCOL_ARITY:
            raise RuntimeError(f"unknown response record from cluster: {kind!r}")
        arity = PROTOCOL_ARITY[kind]
        if index + arity > len(parts):
            raise RuntimeError(f"truncated {kind} record from cluster")
        fields = [part.decode("utf-8", "replace") for part in parts[index : index + arity]]
        index += arity
        records.append((kind, fields))
    return records


def as_float(value: Any) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def inferred_solver_name(command: str) -> str:
    """Give older calls without a recorded solver a readable fallback name."""
    try:
        words = shlex.split(command)
    except ValueError:
        words = command.split()
    for word in words:
        if word in {'env', 'exec'} or (word.find('=') > 0 and '/' not in word.split('=', 1)[0]):
            continue
        return PurePosixPath(word).name or 'Unknown solver'
    return 'Unknown solver'


def parse_result_rows(rows: Any) -> dict[int, ResultRecord]:
    results: dict[int, ResultRecord] = {}
    for fields in rows:
        if len(fields) < 15 or not fields[0].isdigit():
            continue
        task_id = int(fields[0])
        results[task_id] = ResultRecord(
            task_id=task_id,
            complete=fields[1].lower() == "true",
            status=fields[2],
            return_code=fields[3],
            wall_seconds=as_float(fields[4]),
            cpu_seconds=as_float(fields[5]),
            max_memory_kib=as_float(fields[10]),
            task_key=fields[13],
            archive=fields[14],
            szs_status=fields[15] if len(fields) > 15 else '',
        )
    return results


def parse_csv_results(payload: str) -> dict[int, ResultRecord]:
    return parse_result_rows(csv.reader(io.StringIO(payload)))


def parse_tsv_results(payload: str) -> dict[int, ResultRecord]:
    return parse_result_rows(line.split("\t") for line in payload.splitlines())


def parse_jsonl_results(payload: str) -> dict[int, ResultRecord]:
    results: dict[int, ResultRecord] = {}
    for line in payload.splitlines():
        try:
            item = json.loads(line)
            task_id = int(item["task_id"])
        except (json.JSONDecodeError, KeyError, TypeError, ValueError):
            continue
        results[task_id] = ResultRecord(
            task_id=task_id,
            complete=bool(item.get("complete")),
            status=str(item.get("status", "unknown")),
            return_code=str(item.get("return_code", "")),
            wall_seconds=as_float(item.get("wall_seconds")),
            cpu_seconds=as_float(item.get("cpu_seconds")),
            max_memory_kib=as_float(item.get("max_memory_kib")),
            task_key=str(item.get("task_key", "")),
            archive=str(item.get("archive", "")),
            problem=str(item.get("problem", "")),
            command=str(item.get("command", "")),
        )
    return results


def normalize_szs_result(result: ResultRecord) -> None:
    if result.status != 'solver-error':
        return
    szs = result.szs_status.lower()
    if szs == 'timeout':
        result.status = 'time-limit'
    elif szs == 'countersatisfiable':
        result.status = 'counter-satisfiable'
    elif (szs and result.return_code.isdigit() and int(result.return_code) < 128
          and szs not in {'error', 'oserror', 'inputerror', 'syntaxerror',
                          'semanticerror', 'typeerror', 'usageerror'}):
        result.status = 'ok'


def summarize_results(job: JobSnapshot, results: dict[int, ResultRecord]) -> None:
    completed = [result for result in results.values() if result.complete]
    job.completed = len(completed)
    job.wall_sum = sum(result.wall_seconds or 0.0 for result in completed)
    job.wall_max = max((result.wall_seconds or 0.0 for result in completed), default=0.0)
    counts: dict[str, int] = {}
    unresolved: dict[str, int] = {}
    for result in results.values():
        target = counts if result.complete else unresolved
        normalize_szs_result(result)
        status = result.status
        target[status] = target.get(status, 0) + 1
    job.status_counts = counts
    job.unresolved_counts = unresolved


def parse_summary(job: JobSnapshot, payload: str) -> None:
    for line in payload.splitlines():
        fields = line.split("\t")
        if len(fields) == 2 and fields[0] == "completed":
            job.completed = int(fields[1])
        elif len(fields) == 2 and fields[0] == "wall_sum":
            job.wall_sum = float(fields[1])
        elif len(fields) == 2 and fields[0] == "wall_max":
            job.wall_max = float(fields[1])
        elif len(fields) == 3 and fields[0] == "status":
            job.status_counts[fields[1]] = int(fields[2])
        elif len(fields) == 3 and fields[0] == "unresolved":
            job.unresolved_counts[fields[1]] = int(fields[2])


def parse_slurm_lines(payload: str, source: str) -> list[SlurmRecord]:
    records: list[SlurmRecord] = []
    for line in payload.splitlines():
        fields = line.rstrip("|").split("|")
        if source == "queue" and len(fields) >= 9:
            records.append(
                SlurmRecord(
                    display_id=fields[0],
                    array_job_id=fields[1],
                    array_task_id=fields[2],
                    state=fields[3],
                    elapsed=fields[4],
                    time_left=fields[5],
                    job_name=fields[6],
                    workdir=fields[7],
                    location="|".join(fields[8:]),
                    source=source,
                )
            )
        elif source == "accounting" and len(fields) >= 10:
            job_id = fields[1]
            array_job_id, _, array_task_id = job_id.partition("_")
            records.append(
                SlurmRecord(
                    display_id=job_id or fields[0],
                    array_job_id=array_job_id,
                    array_task_id=array_task_id or "N/A",
                    state=fields[3],
                    elapsed=fields[4],
                    time_left="—",
                    job_name=fields[2],
                    workdir=fields[8],
                    location=fields[9] or "—",
                    source=source,
                    start=fields[5],
                    end=fields[6],
                    submitted=fields[7],
                )
            )
    return records


def parse_snapshot(host: str, data: bytes) -> ClusterSnapshot:
    protocol = decode_protocol(data)
    remote_home = ""
    detail_job = ""
    jobs: dict[str, JobSnapshot] = {}
    pending_metadata: dict[str, str] = {}
    summaries: dict[str, str] = {}
    legacy_overviews: dict[str, str] = {}
    detail_csv: dict[str, str] = {}
    detail_tsv: dict[str, str] = {}
    detail_jsonl: dict[str, str] = {}
    manifests: dict[str, str] = {}
    taskdefs: dict[str, list[list[str]]] = {}
    progress_payloads: dict[str, tuple[str, str]] = {}
    call_payloads: dict[str, list[str]] = {}
    batch_tasks: dict[str, list[tuple[str, str]]] = {}
    submissions: dict[str, tuple[str, str]] = {}
    submission_states: dict[str, str] = {}
    result_overrides: dict[str, dict[int, str]] = {}
    logs: dict[str, dict[str, str]] = {}
    queue_payload = ""
    accounting_payload = ""

    for kind, fields in protocol:
        if kind == "REMOTE_HOME":
            remote_home = fields[0]
        elif kind == "DETAIL":
            detail_job = fields[0]
        elif kind == "JOB":
            job_id, created = fields
            jobs[job_id] = JobSnapshot(
                job_id=job_id,
                remote_dir=f"{remote_home}/{job_id}" if remote_home else job_id,
                created_epoch=as_float(created) or 0.0,
            )
        elif kind == "METADATA":
            pending_metadata[fields[0]] = fields[1]
        elif kind == "SUMMARY":
            summaries[fields[0]] = fields[1]
        elif kind == "LEGACY_RESULTS":
            legacy_overviews[fields[0]] = fields[1]
        elif kind == "CALL_PROGRESS":
            call_payloads.setdefault(fields[0], []).append(fields[1])
        elif kind == "BATCH_TASK":
            batch_tasks.setdefault(fields[0], []).append((fields[1], fields[2]))
        elif kind in {"PROGRESS_CSV", "PROGRESS_TSV"}:
            progress_payloads[fields[0]] = (kind, fields[1])
        elif kind in {"SUBMISSION_CSV", "SUBMISSION_TSV"}:
            submissions[fields[0]] = (kind, fields[1])
        elif kind == "SUBMISSION_STATE":
            submission_states[fields[0]] = fields[1]
        elif kind == "RESULT_OVERRIDE" and fields[1].isdigit():
            result_overrides.setdefault(fields[0], {})[int(fields[1])] = fields[2]
        elif kind == "RESULTS_CSV":
            detail_csv[fields[0]] = fields[1]
        elif kind == "RESULTS_TSV":
            detail_tsv[fields[0]] = fields[1]
        elif kind == "RESULTS_JSONL":
            detail_jsonl[fields[0]] = fields[1]
        elif kind == "MANIFEST":
            manifests[fields[0]] = fields[1]
        elif kind == "TASKDEF":
            taskdefs.setdefault(fields[0], []).append(fields[1:])
        elif kind == "LOG":
            logs.setdefault(fields[0], {})[fields[1]] = fields[2]
        elif kind == "SQUEUE":
            queue_payload = fields[0]
        elif kind == "SACCT":
            accounting_payload = fields[0]

    for job_id, job in jobs.items():
        state_fields = submission_states.get(job_id, '').split()
        if len(state_fields) >= 2 and state_fields[0] in {'submitting', 'complete', 'failed'}:
            job.submission_state = state_fields[0]
            job.submission_count = int(state_fields[1]) if state_fields[1].isdigit() else 0
        try:
            job.metadata = json.loads(pending_metadata.get(job_id, "{}"))
        except json.JSONDecodeError:
            job.metadata = {}
        for batch_id, task_id in batch_tasks.get(job_id, []):
            if batch_id.isdigit() and task_id.isdigit():
                job.task_batches[int(task_id)] = int(batch_id)
        if not job.task_batches:
            for batch_id, task_ids in enumerate(job.metadata.get('planned_batch_tasks', [])):
                for task_id in task_ids:
                    if isinstance(task_id, int):
                        job.task_batches[task_id] = batch_id
        if job_id in summaries:
            parse_summary(job, summaries[job_id])
            for override in result_overrides.get(job_id, {}).values():
                if job.status_counts.get('solver-error', 0):
                    job.status_counts['solver-error'] -= 1
                    if not job.status_counts['solver-error']:
                        del job.status_counts['solver-error']
                    job.status_counts[override] = job.status_counts.get(override, 0) + 1
        elif job_id in legacy_overviews:
            summarize_results(job, parse_jsonl_results(legacy_overviews[job_id]))

        for payload in call_payloads.get(job_id, []):
            for fields in csv.reader(io.StringIO(payload)):
                if len(fields) == 5 and fields[1].isdigit() and fields[4].isdigit():
                    task_id = int(fields[1])
                    job.call_progress[task_id] = BatchProgress(
                        batch_id=int(fields[4]), state=fields[0], task_id=task_id,
                        started_epoch=as_float(fields[2]), updated_epoch=as_float(fields[3]),
                        finished_count=0,
                    )
        progress_kind, progress_payload = progress_payloads.get(job_id, ("", ""))
        progress_rows = (
            csv.reader(io.StringIO(progress_payload))
            if progress_kind == "PROGRESS_CSV"
            else (line.split("\t") for line in progress_payload.splitlines())
        )
        for fields in progress_rows:
            if len(fields) < 5:
                continue
            match = re.search(r"(?:batch|chunk)_(\d+)\.(?:csv|tsv)$", fields[0])
            if not match:
                continue
            job.progress[int(match.group(1))] = BatchProgress(
                batch_id=int(match.group(1)),
                state=fields[1],
                task_id=int(fields[2]) if fields[2].isdigit() else None,
                started_epoch=as_float(fields[3]),
                updated_epoch=as_float(fields[4]),
                finished_count=int(fields[5]) if len(fields) > 5 and fields[5].isdigit() else 0,
            )

        submission_kind, submission_payload = submissions.get(job_id, ("", ""))
        submission_rows = (
            list(csv.reader(io.StringIO(submission_payload)))
            if submission_kind == "SUBMISSION_CSV"
            else [line.split("\t") for line in submission_payload.splitlines()]
        )
        for fields in submission_rows[1:]:
            if len(fields) >= 2 and fields[1].isdigit():
                job.submission_offsets[fields[0]] = int(fields[1])
                if len(fields) >= 5 and fields[4].isdigit():
                    job.submitted_batches.add(int(fields[4]))

        if job_id == detail_job:
            results = parse_tsv_results(detail_tsv.get(job_id, ""))
            results.update(parse_csv_results(detail_csv.get(job_id, "")))
            results.update(parse_jsonl_results(detail_jsonl.get(job_id, "")))
            for result in results.values():
                normalize_szs_result(result)
            for task_id, override in result_overrides.get(job_id, {}).items():
                if task_id in results and results[task_id].status == 'solver-error':
                    results[task_id].status = override
            job.results = results
            if results:
                summarize_results(job, results)

            for values in taskdefs.get(job_id, []):
                task_id, key, solver, problem, command, solver_root = values
                if task_id.isdigit():
                    job.tasks[int(task_id)] = TaskDefinition(
                        task_id=int(task_id),
                        task_key=key,
                        solver=solver,
                        problem=problem,
                        command=command,
                        solver_root=solver_root,
                    )
            for line in manifests.get(job_id, "").splitlines():
                try:
                    item = json.loads(line)
                    task_id = int(item["task_id"])
                except (json.JSONDecodeError, KeyError, TypeError, ValueError):
                    continue
                job.tasks[task_id] = TaskDefinition(
                    task_id=task_id,
                    task_key=str(item.get("task_key", "")),
                    solver=str(item.get("solver") or item.get("system", "")),
                    problem=str(item.get("problem", "")),
                    command=str(item.get("remote_command") or item.get("command", "")),
                    limiter_command=str(item.get("limiter_command", "")),
                    solver_root=str(item.get("solver_root", "")),
                )
            for task_id, result in results.items():
                definition = job.tasks.setdefault(task_id, TaskDefinition(task_id))
                if not definition.problem and result.problem:
                    definition.problem = result.problem
                if not definition.command and result.command:
                    definition.command = result.command
                if not definition.task_key and result.task_key:
                    definition.task_key = result.task_key
            for definition in job.tasks.values():
                if not definition.solver:
                    definition.solver = inferred_solver_name(definition.command)
            job.logs = logs.get(job_id, {})

    all_slurm = parse_slurm_lines(queue_payload, "queue") + parse_slurm_lines(
        accounting_payload, "accounting"
    )
    previous_ids = {}
    for job in jobs.values():
        history = job.metadata.get('previous_job_ids', [])
        for old_id in history if isinstance(history, list) else []:
            if isinstance(old_id, str):
                previous_ids[old_id] = job.job_id
    for record in all_slurm:
        job_id = previous_ids.get(PurePosixPath(record.workdir).name, PurePosixPath(record.workdir).name)
        if job_id in jobs:
            jobs[job_id].slurm.append(record)

    return ClusterSnapshot(
        host=host,
        remote_home=remote_home,
        detail_job=detail_job,
        fetched_epoch=time.time(),
        jobs=sorted(jobs.values(), key=lambda item: item.created_epoch, reverse=True),
    )


def parse_task_output(job_id: str, task_id: int, archive: str, data: bytes) -> TaskOutput:
    parts = data.split(b"\0")
    streams: dict[str, OutputStream] = {}
    labels = {
        "solver": "Solver stdout",
        "solver_stderr": "Solver stderr",
        "limiter_stdout": "Limiter stdout",
        "controller": "Slurmy controller diagnostics",
        "watcher": "Runsolver watcher",
        "variables": "Runsolver variables",
    }
    index = 0
    while index < len(parts) and parts[index]:
        kind = parts[index].decode("utf-8", "replace")
        index += 1
        if kind == "ERROR":
            if index >= len(parts):
                raise RuntimeError("truncated task-output error from cluster")
            raise RuntimeError(parts[index].decode("utf-8", "replace"))
        if kind != "TASK_OUTPUT" or index + 4 > len(parts):
            raise RuntimeError(f"invalid task-output response from cluster: {kind!r}")
        stream_kind = parts[index].decode("utf-8", "replace")
        size_text = parts[index + 1].decode("ascii", "replace")
        truncated_text = parts[index + 2].decode("ascii", "replace")
        encoded = parts[index + 3]
        index += 4
        try:
            raw = base64.b64decode(encoded, validate=True) if encoded else b""
        except ValueError as exc:
            raise RuntimeError(f"invalid base64 for {stream_kind} output") from exc
        streams[stream_kind] = OutputStream(
            name=labels.get(stream_kind, stream_kind),
            content=raw.decode("utf-8", "replace"),
            size=int(size_text) if size_text.isdigit() else len(raw),
            truncated=truncated_text == "true",
            exists=size_text != "-1",
        )
    return TaskOutput(job_id=job_id, task_id=task_id, archive=archive, streams=streams)


class RemoteCollector:
    def __init__(self, host: str, timeout: float) -> None:
        self.host = host
        self.timeout = timeout

    def fetch(self, detail_job: str | None) -> ClusterSnapshot:
        if shutil.which("ssh") is None:
            raise RuntimeError("ssh is not installed or not on PATH")
        command = [
            "ssh",
            "-T",
            "-o",
            "BatchMode=yes",
            "-o",
            f"ConnectTimeout={max(1, int(self.timeout))}",
            "--",
            self.host,
            "bash",
            "-s",
            "--",
            detail_job or "",
        ]
        try:
            process = subprocess.run(
                command,
                input=REMOTE_SNAPSHOT_SCRIPT.encode(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=self.timeout + 20,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"SSH refresh timed out after {self.timeout + 20:g}s") from exc
        if process.returncode != 0:
            message = process.stderr.decode("utf-8", "replace").strip()
            raise RuntimeError(message or f"ssh exited with status {process.returncode}")
        return parse_snapshot(self.host, process.stdout)

    def fetch_problem_text(self, job_id: str, problem: str) -> tuple[str, bool]:
        """Read a packaged problem, never an arbitrary path on the SSH host."""
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', job_id):
            raise ValueError('Invalid job ID.')
        path = Path(problem)
        if not path.is_absolute() or '..' in path.parts:
            raise ValueError('Invalid problem path.')
        script = r'''set -euo pipefail
root=$(realpath -e -- "$HOME/slurmy/jobs/$1/rootfs")
file=$(realpath -e -- "$root/$2")
[[ "$file" == "$root/"* && -f "$file" ]] || exit 2
head -c 1048577 -- "$file"
'''
        command = ['ssh', '-T', '-o', 'BatchMode=yes', '-o',
                   f'ConnectTimeout={max(1, int(self.timeout))}', '--', self.host,
                   'bash', '-s', '--', job_id, problem.lstrip('/')]
        process = subprocess.run(command, input=script.encode(), stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, timeout=self.timeout + 20, check=False)
        if process.returncode:
            raise RuntimeError(process.stderr.decode('utf-8', 'replace').strip() or
                               'The packaged problem could not be read on the cluster.')
        return process.stdout[:1048576].decode('utf-8', 'replace'), len(process.stdout) > 1048576

    def fetch_task_output(self, job_id: str, result: ResultRecord) -> TaskOutput:
        if not result.archive or not result.task_key:
            raise RuntimeError("this task has no saved result output yet")
        command = [
            "ssh",
            "-T",
            "-o",
            "BatchMode=yes",
            "-o",
            f"ConnectTimeout={max(1, int(self.timeout))}",
            "--",
            self.host,
            "bash",
            "-s",
            "--",
            job_id,
            result.archive,
            str(result.task_id),
            result.task_key,
        ]
        try:
            process = subprocess.run(
                command,
                input=REMOTE_TASK_OUTPUT_SCRIPT.encode(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=self.timeout + 30,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"task-output request timed out after {self.timeout + 30:g}s") from exc
        if process.returncode != 0:
            message = process.stderr.decode("utf-8", "replace").strip()
            raise RuntimeError(message or f"ssh exited with status {process.returncode}")
        return parse_task_output(job_id, result.task_id, result.archive, process.stdout)
