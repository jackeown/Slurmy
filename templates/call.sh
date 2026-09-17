#!/usr/bin/env bash
# Per-call limiter, output capture and atomic result publication.
set -euo pipefail
export LC_ALL=C
export CALL_DIR="$JOB_DIR/calls/$1"
source "$CALL_DIR/config.sh"
BATCH_ID=$2
source "$JOB_DIR/csv.sh"
export OMP_NUM_THREADS=$CORES
work=$(mktemp -d "${SLURM_TMPDIR:-${TMPDIR:-/tmp}}/slurmy-call.XXXXXXXX")
stem=$(printf 'task_%09d_%s' "$TASK_ID" "$TASK_KEY")
export SOLVER_LOG="$work/$stem.solver.log"
export WATCHER_LOG="$work/$stem.watcher.log"
export VAR_FILE="$work/$stem.var"
export CONTROLLER_LOG="$work/$stem.controller.log"
export TIMING_LOG="$work/time"
progress="$JOB_DIR/progress/task_$TASK_ID.csv"
started=$(date +%s)
publish_progress() {
    csv_row "$1" "$TASK_ID" "$started" "$(date +%s)" "$BATCH_ID" > "$progress.tmp"
    mv "$progress.tmp" "$progress"
}
publish_progress running
active=
interrupted=0
stop() { interrupted=1; [[ -z "$active" ]] || kill -TERM -- "-$active" 2>/dev/null || true; }
trap stop TERM INT
trap 'rm -rf -- "$work"' EXIT
cd "$JOB_DIR/rootfs/$SOLVER_ROOT_REL"
# A session lets cancellation terminate the full limiter/solver process group.
# The outer timeout catches a broken limiter while allowing its cleanup buffer.
setsid timeout --signal=TERM --kill-after=5 "$((WC_LIMIT + 25))" \
    bash "$JOB_DIR/timed_call.sh" > "$SOLVER_LOG" 2> "$CONTROLLER_LOG" &
active=$!
set +e
wait "$active"
code=$?
if (( interrupted )); then
    kill -KILL -- "-$active" 2>/dev/null || true
    wait "$active" 2>/dev/null
fi
set -e
active=
wall=$(( $(date +%s) - started )); user=; system=; memory=; cpu=; max_vm=; cpu_usage=
if [[ -f "$work/time" ]]; then
    timing=$(tail -n 1 "$work/time")
    if [[ "$timing" =~ ^[0-9] ]]; then
        read -r wall user system memory <<< "$timing"
        cpu=$(awk -v u="$user" -v s="$system" 'BEGIN {print u+s}')
    fi
fi
timed_out=false; memory_out=false; status=ok; complete=true
if [[ -f "$WATCHER_LOG" ]]; then
    child=$(sed -n 's/^Child status:[[:space:]]*//p' "$WATCHER_LOG" | tail -n 1)
    [[ ! "$child" =~ ^[0-9]+$ ]] || code=$child
fi
if [[ -f "$VAR_FILE" ]]; then
    timed_out=$(awk -F= '$1=="TIMEOUT" {print $2}' "$VAR_FILE")
    memory_out=$(awk -F= '$1=="MEMOUT" {print $2}' "$VAR_FILE")
    # Runsolver measures the solver tree rather than the limiter's overhead.
    read_var() { awk -F= -v key="$1" '$1==key {print $2}' "$VAR_FILE"; }
    actual_cpu=$(read_var CPUTIME)
    [[ -z "$actual_cpu" ]] || cpu=$actual_cpu
    actual_wall=$(read_var WCTIME)
    [[ -z "$actual_wall" ]] || wall=$actual_wall
    user=$(read_var USERTIME); system=$(read_var SYSTEMTIME)
    max_vm=$(read_var MAXVM); cpu_usage=$(read_var CPUUSAGE)
fi
if [[ -f "$WATCHER_LOG" ]]; then
    memory=$(sed -n 's/^Max\. memory (cumulated for all children) (KiB):[[:space:]]*//p' "$WATCHER_LOG" | tail -n 1)
    if [[ -z "$memory" || "$memory" == 0 ]]; then
        memory=$(sed -n 's/^maximum resident set size=[[:space:]]*//p' "$WATCHER_LOG" | tail -n 1)
    fi
fi
if (( interrupted )); then status=interrupted; complete=false
elif [[ "$memory_out" == true ]]; then status=memory-limit
elif [[ "$timed_out" == true ]]; then status=time-limit
elif (( code == 124 || code == 137 )); then status=worker-error; complete=false
elif (( code != 0 )); then status=error
fi
archive="batch_${BATCH_ID}_${stem}_${SLURM_JOB_ID:-local}_$(date +%s).tar.gz"
tar -czf "$JOB_DIR/results/.$archive.tmp" -C "$work" .
mv "$JOB_DIR/results/.$archive.tmp" "$JOB_DIR/results/$archive"
result=$(printf '%s/results/batch_%06d_task_%09d.csv' "$JOB_DIR" "$BATCH_ID" "$TASK_ID")
{
    csv_row task_id complete status return_code wall_seconds cpu_seconds user_seconds system_seconds cpu_usage_percent max_virtual_memory_kib max_memory_kib timed_out memory_out task_key archive
    csv_row "$TASK_ID" "$complete" "$status" "$code" "$wall" "$cpu" "$user" "$system" "$cpu_usage" "$max_vm" "$memory" "${timed_out:-false}" "${memory_out:-false}" "$TASK_KEY" "$archive"
} > "$result.tmp"
mv "$result.tmp" "$result"
publish_progress "$status"
[[ "$complete" == true ]]
