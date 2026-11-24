#!/usr/bin/env bash
# Per-call limiter, output capture and atomic result publication.
set -euo pipefail
export LC_ALL=C
export CALL_DIR="${CALLS_DIR:-$JOB_DIR/calls}/$1"
source "$CALL_DIR/config.sh"
source "$JOB_DIR/szs.sh"
if [[ -n ${TPTP_ROOT_REL:-} ]]; then
    export TPTP="$JOB_DIR/rootfs/$TPTP_ROOT_REL"
elif [[ -n ${TPTP_DEFAULT_ROOT:-} ]]; then
    if [[ -d "$TPTP_DEFAULT_ROOT/Axioms" && -r "$TPTP_DEFAULT_ROOT/Axioms" && -x "$TPTP_DEFAULT_ROOT/Axioms" ]]; then
        export TPTP="$TPTP_DEFAULT_ROOT"
    else
        printf 'Warning: shared TPTP axioms are not accessible at %s/Axioms; TPTP include files may fail.\n' "$TPTP_DEFAULT_ROOT" >&2
    fi
fi
BATCH_ID=$2
source "$JOB_DIR/csv.sh"
export OMP_NUM_THREADS=$((CORES * THREADS_PER_CORE))
work=$(mktemp -d "${SLURM_TMPDIR:-${TMPDIR:-/tmp}}/slurmy-call.XXXXXXXX")
stem=$(printf 'task_%09d_%s' "$TASK_ID" "$TASK_KEY")
export SOLVER_LOG="$work/$stem.solver.log"
export SOLVER_STDOUT_LOG="$work/$stem.solver-stdout.log"
export SOLVER_STDERR_LOG="$work/$stem.solver-stderr.log"
export LIMITER_STDOUT_LOG="$work/$stem.limiter-stdout.log"
export WATCHER_LOG="$work/$stem.watcher.log"
export VAR_FILE="$work/$stem.var"
export CONTROLLER_LOG="$work/$stem.controller.log"
export TIMING_LOG="$work/time"
progress="$JOB_DIR/progress/task_$TASK_ID.csv"
batch_progress=$(printf '%s/progress/batch_%06d.csv' "$JOB_DIR" "$BATCH_ID")
started=$(date +%s)
publish_progress() {
    csv_row "$1" "$TASK_ID" "$started" "$(date +%s)" "$BATCH_ID" > "$progress.tmp"
    mv "$progress.tmp" "$progress"
    csv_row "$1" "$TASK_ID" "$started" "$(date +%s)" 0 > "$batch_progress.tmp"
    mv "$batch_progress.tmp" "$batch_progress"
}
publish_progress running
active=
interrupted=0
stop() { interrupted=1; [[ -z "$active" ]] || kill -TERM -- "-$active" 2>/dev/null || true; }
trap stop TERM INT
trap 'rm -rf -- "$work"' EXIT
cd "$JOB_DIR/rootfs/$SOLVER_ROOT_REL"
# Collect host information outside the solver's measured time and limits.
# Runsolver truncates solver-data, so prepend this report after it finishes.
if [[ ${GET_COMPUTER_INFO:-0} == 1 ]]; then
    info_args=(-p --prefix '% ' --cpu-limit "$CPU_LIMIT" --wc-limit "$WC_LIMIT" --mem-limit "${MEM_LIMIT_MIB}MiB")
    [[ ${COMPUTER_INFO_TIMESTAMP:-0} != 1 ]] || info_args+=(--timestamp)
    setsid timeout --signal=TERM --kill-after=5 45 bash "$JOB_DIR/GetComputerInfo" \
        "${info_args[@]}" THIS Model CPUModel RAMPerCPU OS CPULimit WCLimit MemLimit DateTime > "$work/computer-info.header" 2>> "$CONTROLLER_LOG" &
    active=$!
    info_code=0
    wait "$active" || info_code=$?
    if (( interrupted )); then
        kill -KILL -- "-$active" 2>/dev/null || true
        wait "$active" 2>/dev/null || true
    elif (( info_code != 0 )); then
        printf 'Warning: GetComputerInfo exited with status %s; continuing with the solver.\n' "$info_code" >> "$CONTROLLER_LOG"
    fi
    active=
fi
# A session lets cancellation terminate the full limiter/solver process group.
# The outer timeout catches a broken limiter while allowing its cleanup buffer.
code=143
if (( ! interrupted )); then
# GNU timeout's duration 0 disables its watchdog for unlimited wall time.
watchdog=0
(( WC_LIMIT == 0 )) || watchdog=$((WC_LIMIT + 25))
setsid timeout --signal=TERM --kill-after=5 "$watchdog" \
    bash "$JOB_DIR/timed_call.sh" > "$LIMITER_STDOUT_LOG" 2>> "$CONTROLLER_LOG" &
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
fi
if [[ -f "$SOLVER_STDOUT_LOG" ]]; then
    # Runsolver's solver-data is the live combined stream. Publish raw stdout
    # separately, so inspecting/downloading stderr never duplicates it there.
    [[ ! -f "$SOLVER_LOG" ]] || mv "$SOLVER_LOG" "$work/$stem.solver-combined.log"
    mv "$SOLVER_STDOUT_LOG" "$SOLVER_LOG"
fi
if [[ -f "$work/computer-info.header" ]]; then
    # The host report belongs to the full run, never to raw prover stdout.
    combined="$work/$stem.solver-combined.log"
    if [[ ! -f "$combined" && -f "$SOLVER_LOG" ]]; then cp "$SOLVER_LOG" "$combined"; fi
    { cat "$work/computer-info.header"; [[ ! -f "$combined" ]] || cat "$combined"; } > "$work/solver-with-info"
    mv "$work/solver-with-info" "$combined"
    rm "$work/computer-info.header"
fi
wall=$(( $(date +%s) - started )); user=; system=; memory=; cpu=; max_vm=; cpu_usage=
if [[ -f "$work/time" ]]; then
    timing=$(tail -n 1 "$work/time")
    if [[ "$timing" =~ ^[0-9] ]]; then
        read -r wall user system memory <<< "$timing"
        cpu=$(awk -v u="$user" -v s="$system" 'BEGIN {print u+s}')
    fi
fi
timed_out=false; memory_out=false; status=ok; complete=true; child=
limiter_code=$code
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
if [[ -f "$WATCHER_LOG" ]] && grep -Fq 'Maximum memory exceeded:' "$WATCHER_LOG"; then
    # Some runsolver versions write MEMOUT=false even after reporting the
    # limit breach and terminating the child. The watcher is authoritative.
    memory_out=true
fi
szs_status=
szs_status=$(sed -nE 's/.*SZS[[:space:]]+status[[:space:]]+([A-Za-z][A-Za-z0-9_-]*(\([[:space:]]*[A-Za-z]+[[:space:]]*,[[:space:]]*[A-Za-z]+[[:space:]]*\))?).*/\1/Ip' \
    "$SOLVER_LOG" "$SOLVER_STDERR_LOG" "$LIMITER_STDOUT_LOG" 2>/dev/null | tail -n 1 || true)
szs_output=$(sed -nE 's/.*SZS[[:space:]]+output[[:space:]]+(start|end)[[:space:]]+([A-Za-z][A-Za-z0-9_-]*).*/\2/Ip' \
    "$SOLVER_LOG" "$SOLVER_STDERR_LOG" "$LIMITER_STDOUT_LOG" 2>/dev/null | tail -n 1 || true)
# Solver-enforced limits can fire before the external limiter. Vampire's
# non-portfolio mode reports a termination reason without an SZS status.
szs_status=$(szs_status_name "$szs_status")
szs_output=$(szs_data_name "$szs_output")
szs_kind=$(szs_execution "$szs_status")
case "$szs_kind" in
    time-limit) timed_out=true ;;
    memory-limit) memory_out=true ;;
esac
if [[ -z "$szs_status" ]]; then
    if grep -Eq '(^|[[:space:]])%[[:space:]]+Termination reason:[[:space:]]+Time limit[[:space:]]*$' "$SOLVER_LOG" "$SOLVER_STDERR_LOG" 2>/dev/null; then
        timed_out=true
    elif grep -Eq '(^|[[:space:]])%[[:space:]]+Termination reason:[[:space:]]+Memory limit[[:space:]]*$' "$SOLVER_LOG" "$SOLVER_STDERR_LOG" 2>/dev/null; then
        memory_out=true
    fi
fi
analysis_header=; analysis_value=
if [[ -n ${ANALYSIS_SCRIPT:-} ]]; then
    source "$JOB_DIR/analysis-settings.sh"
    analysis_header=",$(csv_row analysis)"
    analysis_value=",$(csv_row '')"
    if (( ! interrupted )) && [[ -f "$SOLVER_LOG" || -f "$SOLVER_STDERR_LOG" ]]; then
        setsid timeout --signal=TERM --kill-after="$ANALYSIS_KILL_GRACE" "$ANALYSIS_TIMEOUT" \
            python3 "$JOB_DIR/analyze_run.py" "$JOB_DIR/$ANALYSIS_SCRIPT" "$ANALYSIS_COLUMNS" "$SOLVER_LOG" "$SOLVER_STDERR_LOG" \
            > "$work/analysis.json" 2>> "$CONTROLLER_LOG" &
        active=$!
        analysis_code=0
        wait "$active" || analysis_code=$?
        if (( interrupted )); then
            kill -KILL -- "-$active" 2>/dev/null || true
            wait "$active" 2>/dev/null || true
        elif (( analysis_code == 0 )); then
            analysis_value=",$(csv_row "$(< "$work/analysis.json")")"
        else
            printf 'Analysis failed for solver-problem pair %s (exit %s).\n' "$TASK_ID" "$analysis_code" | tee -a "$CONTROLLER_LOG" >&2
        fi
        active=
        rm -f -- "$work/analysis.json"
    fi
fi
if (( interrupted )); then status=interrupted; complete=false
elif [[ "$memory_out" == true ]]; then status=memory-limit
elif [[ "$timed_out" == true ]]; then status=time-limit
elif (( code == 124 || code == 137 )); then status=worker-error; complete=false
elif (( limiter_code != 0 )) && [[ -z "$child" ]]; then status=resource-limiter-error
elif (( code >= 128 )); then status=solver-error
elif [[ -n "$szs_kind" ]]; then status=$szs_kind
elif (( code != 0 )); then status=solver-error
fi
archive="batch_${BATCH_ID}_${stem}_${SLURM_JOB_ID:-local}_$(date +%s).tar.gz"
tar -czf "$JOB_DIR/results/.$archive.tmp" -C "$work" .
mv "$JOB_DIR/results/.$archive.tmp" "$JOB_DIR/results/$archive"
result=$(printf '%s/results/batch_%06d_task_%09d.csv' "$JOB_DIR" "$BATCH_ID" "$TASK_ID")
header=$(csv_row task_id complete status return_code wall_seconds cpu_seconds user_seconds system_seconds cpu_usage_percent max_virtual_memory_kib max_memory_kib timed_out memory_out task_key archive szs_status szs_output)
result_row=$(csv_row "$TASK_ID" "$complete" "$status" "$code" "$wall" "$cpu" "$user" "$system" "$cpu_usage" "$max_vm" "$memory" "${timed_out:-false}" "${memory_out:-false}" "$TASK_KEY" "$archive" "$szs_status" "$szs_output")
printf '%s%s\n' "$header" "$analysis_header" "$result_row" "$analysis_value" > "$result.tmp"
mv "$result.tmp" "$result"
# One small, atomically replaced index per sequential batch keeps monitoring
# fast on NFS; individual result files remain available for sync and recovery.
index=$(printf '%s/results/index/batch_%06d.csv' "$JOB_DIR" "$BATCH_ID")
if {
    if [[ -f "$index" ]]; then
        # A resumed call replaces its earlier incomplete row in the compact
        # monitor index; otherwise it would be counted twice.
        awk -F, -v id="$TASK_ID" '{field=$1; gsub(/"/, "", field); if (NR==1 || field!=id) print}' "$index"
    else
        printf '%s\n' "$header"
    fi
    printf '%s\n' "$result_row"
} > "$index.tmp" && mv "$index.tmp" "$index"; then
    :
else
    printf 'Warning: could not update result index for batch %s. Individual result is saved.\n' "$BATCH_ID" >&2
    rm -f -- "$index.tmp" "$JOB_DIR/results/index/.ready" || true
fi
publish_progress "$status"
[[ "$complete" == true ]]
