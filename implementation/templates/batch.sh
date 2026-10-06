#!/usr/bin/env bash
# One wave of calls. Select disjoint physical cores from this batch's cpuset.
# Full-socket requests are verified before ANY solver starts, including all
# sibling hardware threads, rather than trusting a core count to imply locality.
set -euo pipefail
export JOB_DIR=${SLURM_SUBMIT_DIR:?}
cd "$JOB_DIR"
source ./csv.sh
BATCH_ID=${SLURMY_BATCH_ID:-${SLURM_ARRAY_TASK_ID:?}}
source "$(printf 'batches/batch_%06d.sh' "$BATCH_ID")"
(( PARALLEL == 1 )) || { echo 'Slurmy batches must run one call at a time.' >&2; exit 1; }
declare -A ALLOWED=() CORE_CPU=() CORE_SOCKET=() SOCKET_TOTAL=() SOCKET_ALLOWED=() USED=()
allowed_list=$(awk '/^Cpus_allowed_list:/ {print $2}' /proc/self/status)
IFS=, read -r -a ranges <<< "$allowed_list"
for range in "${ranges[@]}"; do
    first=${range%-*}; last=${range#*-}
    for ((cpu=first; cpu<=last; cpu++)); do ALLOWED[$cpu]=1; done
done
while IFS=, read -r cpu core socket; do
    [[ "$cpu" =~ ^[0-9]+$ ]] || continue
    key="$socket:$core"
    SOCKET_TOTAL[$socket]=$(( ${SOCKET_TOTAL[$socket]:-0} + 1 ))
    if [[ ${ALLOWED[$cpu]:-} ]]; then
        SOCKET_ALLOWED[$socket]=$(( ${SOCKET_ALLOWED[$socket]:-0} + 1 ))
        CORE_CPU[$key]=${CORE_CPU[$key]:-$cpu}
        CORE_SOCKET[$key]=$socket
    fi
done < <(lscpu -p=CPU,CORE,SOCKET)
mapfile -t core_keys < <(printf '%s\n' "${!CORE_CPU[@]}" | sort -t: -k1,1n -k2,2n)
mapfile -t sockets < <(printf '%s\n' "${!SOCKET_TOTAL[@]}" | sort -n)
if (( ! EXCLUSIVE_NODE && ${#CORE_CPU[@]} > RESERVED_CORES )); then
    echo 'CPU containment exposes more cores than requested. Ask the administrator to enable ConstrainCores=yes; no calls started.' >&2
    exit 1
fi
placement_failure() {
    local task_id=$1 need=$2 socket_limit=$3 wave_start=$4
    if (( wave_start == 0 )) && [[ ${SLURM_RESTART_COUNT:-0} =~ ^[0-9]+$ ]] && (( ${SLURM_RESTART_COUNT:-0} < 5 )); then
        local array_id="${SLURM_ARRAY_JOB_ID:?}_${SLURM_ARRAY_TASK_ID:?}"
        echo "Batch $BATCH_ID: allocated cores cannot satisfy jobpair $task_id (cores=$need sockets=$socket_limit exclusive_cpu=$EXCLUSIVE_CPU). No calls started; requeuing $array_id for another placement (retry $((${SLURM_RESTART_COUNT:-0} + 1))/5)." >&2
        if scontrol requeue "$array_id"; then exit 0; fi
        echo "Batch $BATCH_ID: Slurm refused the requeue; no calls started." >&2
    elif (( wave_start == 0 )); then
        echo "Batch $BATCH_ID: allocated cores still cannot satisfy jobpair $task_id (cores=$need sockets=$socket_limit exclusive_cpu=$EXCLUSIVE_CPU) after placement retries. No calls started; adjust the call's socket/core request or batch concurrency." >&2
    else
        echo "Batch $BATCH_ID: allocated cores cannot satisfy jobpair $task_id (cores=$need sockets=$socket_limit exclusive_cpu=$EXCLUSIVE_CPU). Earlier waves may have completed, so this batch will not be requeued." >&2
    fi
    exit 1
}
failed=0
for ((first=0; first<${#TASK_IDS[@]}; first+=PARALLEL)); do
USED=()
MASKS=()
last=$((first + PARALLEL))
(( last <= ${#TASK_IDS[@]} )) || last=${#TASK_IDS[@]}
for ((index=first; index<last; index++)); do
    need=${TASK_CORES[$index]}; socket_limit=${TASK_CPUS[$index]}
    selected=(); reserved=(); used_sockets=0
    for socket in "${sockets[@]}"; do
        (( used_sockets < socket_limit )) || break
        candidates=(); busy=0
        for key in "${core_keys[@]}"; do
            [[ ${CORE_SOCKET[$key]} == "$socket" ]] || continue
            if [[ ${USED[$key]:-} ]]; then busy=1; else candidates+=("$key"); fi
        done
        if (( EXCLUSIVE_CPU )); then
            (( busy == 0 && ${SOCKET_ALLOWED[$socket]:-0} == ${SOCKET_TOTAL[$socket]} )) || continue
            reserved+=("${candidates[@]}")
        fi
        (( ${#candidates[@]} )) || continue
        used_sockets=$((used_sockets + 1))
        for key in "${candidates[@]}"; do
            if (( ${#selected[@]} < need )); then selected+=("$key"); fi
        done
        if (( ! EXCLUSIVE_CPU && ${#selected[@]} == need )); then break; fi
    done
    if (( ${#selected[@]} != need || (EXCLUSIVE_CPU && used_sockets != socket_limit) )); then
        placement_failure "${TASK_IDS[$index]}" "$need" "$socket_limit" "$first"
    fi
    if (( ! EXCLUSIVE_CPU )); then reserved=("${selected[@]}"); fi
    mask=
    for key in "${selected[@]}"; do mask+="${mask:+,}${CORE_CPU[$key]}"; done
    for key in "${reserved[@]}"; do USED[$key]=1; done
    MASKS+=("$mask")
done
PIDS=()
stop() {
    trap '' TERM INT
    for pid in "${PIDS[@]}"; do kill -TERM "$pid" 2>/dev/null || true; done
    wait || true
    exit 143
}
trap stop TERM INT
for ((index=first; index<last; index++)); do
    taskset -c "${MASKS[$((index-first))]}" bash "$JOB_DIR/call.sh" "${TASK_IDS[$index]}" "$BATCH_ID" &
    PIDS+=("$!")
done
for pid in "${PIDS[@]}"; do wait "$pid" || failed=1; done
done
exit "$failed"
