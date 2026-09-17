#!/usr/bin/env bash
# Run on the head node: validate scheduling policy and topology, then sbatch.
set -euo pipefail
JOB_DIR="$HOME/Slurmy/$1"
PARTITION=$2
mkdir -p "$JOB_DIR/rootfs" "$JOB_DIR/results" "$JOB_DIR/progress" "$JOB_DIR/logs"
tar -xzf "$JOB_DIR/incoming/job-files.tar.gz" -C "$JOB_DIR"
tar -xzf "$JOB_DIR/incoming/inputs.tar.gz" -C "$JOB_DIR/rootfs"
cd "$JOB_DIR"
source ./csv.sh

# Physical core isolation requires allocation at core granularity and cgroups.
CONFIG=$(scontrol show config)
grep -Eq 'SelectTypeParameters[[:space:]]*=.*CR_CORE' <<< "$CONFIG" || {
    echo 'This scheduler currently requires SelectTypeParameters=CR_CORE[_MEMORY].' >&2; exit 1;
}
grep -Eq 'TaskPlugin[[:space:]]*=.*task/cgroup' <<< "$CONFIG" || {
    echo 'CPU containment requires task/cgroup on this cluster.' >&2; exit 1;
}
PARTITION_INFO=$(scontrol show partition -o "$PARTITION")
[[ "$PARTITION_INFO" != *OverSubscribe=FORCE* ]] || {
    echo 'Selected partition forces CPU sharing; exclusive cores cannot be guaranteed.' >&2; exit 1;
}
# Require a homogeneous topology rather than silently guessing a socket size.
TOPOLOGY=$(sinfo -p "$PARTITION" -N -h -o '%X %Y %Z %m' | sort -u)
[[ $(wc -l <<< "$TOPOLOGY") == 1 ]] || {
    echo 'Select a partition with homogeneous socket/core/thread topology.' >&2; exit 1;
}
read -r SOCKETS CORES_PER_SOCKET THREADS_PER_CORE NODE_MEMORY <<< "$TOPOLOGY"
[[ "$SOCKETS $CORES_PER_SOCKET $THREADS_PER_CORE" =~ ^[1-9][0-9]*\ [1-9][0-9]*\ [1-9][0-9]*$ ]] || exit 1
csv_row batch_id concurrent_calls reserved_cores memory_mib wall_seconds exclusive_cpu exclusive_node > allocations.csv

# Split proposed waves further to fit actual hardware. Never increase deg_par.
mkdir -p batches
next_batch=0
for plan in plans/batch_*.sh; do
    source "$plan"
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    SLOT_MEMORY=$((MEMORY_MIB / ${#TASK_IDS[@]}))
    capacity=$((SOCKETS * CORES_PER_SOCKET / SLOT_CORES))
    memory_capacity=$((NODE_MEMORY / SLOT_MEMORY))
    (( capacity <= memory_capacity )) || capacity=$memory_capacity
    if (( EXCLUSIVE_CPU )); then
        socket_capacity=$((SOCKETS / MAX_CPUS))
        (( capacity <= socket_capacity )) || capacity=$socket_capacity
    fi
    (( capacity > 0 && MAX_CPUS <= SOCKETS && MAX_CORES <= MAX_CPUS * CORES_PER_SOCKET )) || {
        echo "A jobpair in $plan cannot fit this partition's nodes." >&2; exit 1;
    }
    for ((first=0; first<${#TASK_IDS[@]}; first+=capacity)); do
        count=$capacity
        (( first + count <= ${#TASK_IDS[@]} )) || count=$(( ${#TASK_IDS[@]} - first ))
        file=$(printf 'batches/batch_%06d.sh' "$next_batch")
        wall=60
        for ((i=first; i<first+count; i++)); do wall=$((wall + TASK_WALL[i] + 30)); done
        {
            printf 'TASK_IDS=('; printf ' %q' "${TASK_IDS[@]:first:count}"; printf ' )\n'
            printf 'TASK_CORES=('; printf ' %q' "${TASK_CORES[@]:first:count}"; printf ' )\n'
            printf 'TASK_CPUS=('; printf ' %q' "${TASK_CPUS[@]:first:count}"; printf ' )\n'
            printf 'BATCH_ID=%s\nEXCLUSIVE_CPU=%s\nEXCLUSIVE_NODE=%s\nMAX_CORES=%s\nMAX_CPUS=%s\nMEMORY_MIB=%s\nWALL_SECONDS=%s\n' \
                "$next_batch" "$EXCLUSIVE_CPU" "$EXCLUSIVE_NODE" "$MAX_CORES" "$MAX_CPUS" "$((count * SLOT_MEMORY))" "$wall"
            printf 'RESERVED_CORES=%s\n' "$((count * SLOT_CORES))"
        } > "$file"
        next_batch=$((next_batch + 1))
    done
done
sed -i -E "s/\"batch_count\": [0-9]+/\"batch_count\": $next_batch/" metadata.json

# Validate every batch before submitting any of them.
for batch in batches/batch_*.sh; do
    source "$batch"
    PARALLEL=${#TASK_IDS[@]}
    (( MAX_CPUS <= SOCKETS && MAX_CORES <= MAX_CPUS * CORES_PER_SOCKET )) || {
        echo "Batch $BATCH_ID exceeds the node's socket/core capacity." >&2; exit 1;
    }
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    (( PARALLEL * SLOT_CORES <= SOCKETS * CORES_PER_SOCKET )) || {
        echo "Batch $BATCH_ID cannot fit $PARALLEL concurrent calls on a node; reduce --deg_par." >&2; exit 1;
    }
    csv_row "$BATCH_ID" "$PARALLEL" "$((PARALLEL * SLOT_CORES))" "$MEMORY_MIB" "$WALL_SECONDS" "$EXCLUSIVE_CPU" "$EXCLUSIVE_NODE" >> allocations.csv
done
csv_row slurm_job_id offset array_size submitted_epoch > submission.csv
for batch in batches/batch_*.sh; do
    source "$batch"
    PARALLEL=${#TASK_IDS[@]}
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    OPTIONS=(--parsable --partition="$PARTITION" --job-name=Slurmy
        --nodes=1 --ntasks="$PARALLEL" --cpus-per-task="$SLOT_CORES"
        --threads-per-core=1 --distribution=block:block
        --mem="${MEMORY_MIB}M" --time="$(( (WALL_SECONDS + 59) / 60 ))"
        --array="$BATCH_ID-$BATCH_ID" --output=logs/slurm_%A_%a.out
        --signal=B:TERM@30)
    if (( EXCLUSIVE_CPU )); then
        # With CR_CORE, reject partially occupied sockets during selection.
        # A socket-sized core count alone can otherwise span two partial sockets.
        OPTIONS+=(--ntasks-per-socket=1 --cores-per-socket="$CORES_PER_SOCKET"
            --sockets-per-node="$((PARALLEL * MAX_CPUS))")
    fi
    if (( EXCLUSIVE_NODE )); then OPTIONS+=(--exclusive); fi
    SUBMITTED=$(sbatch "${OPTIONS[@]}" batch.sh)
    ID=${SUBMITTED%%;*}
    [[ "$ID" =~ ^[0-9]+$ ]] || { echo "Invalid sbatch ID: $SUBMITTED" >&2; exit 1; }
    csv_row "$ID" 0 1 "$(date +%s)" >> submission.csv
    echo "Batch $BATCH_ID: Slurm $ID"
done
echo "Slurmy job ID: $1"
echo "Remote directory: $JOB_DIR"
