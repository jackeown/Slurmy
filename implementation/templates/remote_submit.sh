#!/usr/bin/env bash
# Run on the head node: validate scheduling policy and topology, then sbatch.
set -euo pipefail
JOB_DIR="$HOME/slurmy/jobs/$1"
export JOB_DIR
PARTITION=$2
JOB_PREFIX=$4
[[ "$JOB_PREFIX" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || {
    echo 'Invalid job prefix.' >&2; exit 1;
}
mkdir -p "$JOB_DIR/rootfs" "$JOB_DIR/results" "$JOB_DIR/progress" "$JOB_DIR/logs"
tar -xzf "$JOB_DIR/incoming/job-files.tar.gz" -C "$JOB_DIR"
tar -xzf "$JOB_DIR/incoming/inputs.tar.gz" -C "$JOB_DIR/rootfs"
cd "$JOB_DIR"
python3 - "${5:-}" <<'PY'
import base64
import json
from pathlib import Path
import sys

name = base64.b64decode(sys.argv[1], validate=True).decode('utf-8')
if not name.strip() or len(name) > 100 or any(ord(char) < 32 or ord(char) == 127 for char in name):
    raise SystemExit('Invalid job name.')
path = Path('metadata.json')
metadata = json.loads(path.read_text(encoding='utf-8'))
metadata['job_name'] = name.strip()
metadata['job_id'] = Path.cwd().name
temporary = path.with_suffix('.tmp')
temporary.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
temporary.replace(path)
PY
if [[ -f ./prepare_axioms.sh ]]; then bash ./prepare_axioms.sh; fi
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

# Resolve automatic socket needs using the cluster's actual node topology.
mkdir -p batches
next_batch=0
for plan in plans/batch_*.sh; do
    source "$plan"
    MAX_CPUS=0
    for ((i=0; i<${#TASK_IDS[@]}; i++)); do
        if (( TASK_CPUS[i] == 0 )); then
            if (( EXCLUSIVE_CPU )); then
                TASK_CPUS[i]=$(( (TASK_CORES[i] + CORES_PER_SOCKET - 1) / CORES_PER_SOCKET ))
            else
                TASK_CPUS[i]=$SOCKETS
            fi
        fi
        (( MAX_CPUS >= TASK_CPUS[i] )) || MAX_CPUS=${TASK_CPUS[i]}
    done
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
    # A Slurm task owns the full planned batch; only PARALLEL calls run at once.
    for ((first=0; first<${#TASK_IDS[@]}; first+=${#TASK_IDS[@]})); do
        count=${#TASK_IDS[@]}
        parallel=1
        file=$(printf 'batches/batch_%06d.sh' "$next_batch")
        wall=60
        for ((i=first; i<first+count; i++)); do wall=$((wall + TASK_WALL[i] + 30)); done
        {
            printf 'TASK_IDS=('; printf ' %q' "${TASK_IDS[@]:first:count}"; printf ' )\n'
            printf 'TASK_CORES=('; printf ' %q' "${TASK_CORES[@]:first:count}"; printf ' )\n'
            printf 'TASK_CPUS=('; printf ' %q' "${TASK_CPUS[@]:first:count}"; printf ' )\n'
            printf 'BATCH_ID=%s\nPARALLEL=%s\nEXCLUSIVE_CPU=%s\nEXCLUSIVE_NODE=%s\nMAX_CORES=%s\nMAX_CPUS=%s\nMEMORY_MIB=%s\nWALL_SECONDS=%s\n' \
                "$next_batch" "$parallel" "$EXCLUSIVE_CPU" "$EXCLUSIVE_NODE" "$MAX_CORES" "$MAX_CPUS" "$((parallel * SLOT_MEMORY))" "$wall"
            printf 'RESERVED_CORES=%s\n' "$((parallel * SLOT_CORES))"
        } > "$file"
        next_batch=$((next_batch + 1))
    done
done
sed -i -E "s/\"batch_count\": [0-9]+/\"batch_count\": $next_batch/" metadata.json

# Validate every batch before submitting any of them.
for batch in batches/batch_*.sh; do
    source "$batch"
    PARALLEL=${PARALLEL:-${#TASK_IDS[@]}}
    (( MAX_CPUS <= SOCKETS && MAX_CORES <= MAX_CPUS * CORES_PER_SOCKET )) || {
        echo "Batch $BATCH_ID exceeds the node's socket/core capacity." >&2; exit 1;
    }
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    (( PARALLEL * SLOT_CORES <= SOCKETS * CORES_PER_SOCKET )) || {
        echo "Batch $BATCH_ID cannot fit its call on a node; reduce its core request." >&2; exit 1;
    }
    csv_row "$BATCH_ID" "$PARALLEL" "$((PARALLEL * SLOT_CORES))" "$MEMORY_MIB" "$WALL_SECONDS" "$EXCLUSIVE_CPU" "$EXCLUSIVE_NODE" >> allocations.csv
done
csv_row slurm_job_id offset array_size submitted_epoch batch_id > submission.csv
# Keep this controller connected until every batch is accepted. A modest window
# leaves room for the user's other jobs; Slurm's association limit may be lower,
# so an explicit submit-limit rejection is retried after jobs finish.
MAX_OUTSTANDING=${3:-32}
[[ "$MAX_OUTSTANDING" =~ ^[1-9][0-9]*$ ]] || {
    echo 'SLURMY_MAX_OUTSTANDING must be a positive integer.' >&2; exit 1;
}
submitted=0
printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
trap 'rc=$?; if (( rc != 0 )); then printf "failed %s %s\n" "$submitted" "$next_batch" > submission.state; fi' EXIT
trap 'exit 1' HUP INT TERM
echo "Slurmy job ID: $1"
echo "Submission progress: $submitted/$next_batch batches accepted (at most $MAX_OUTSTANDING outstanding)"
OUTSTANDING_IDS=()
refresh_outstanding() {
    local ids output
    (( ${#OUTSTANDING_IDS[@]} )) || return 0
    ids=$(IFS=,; echo "${OUTSTANDING_IDS[*]}")
    output=$(squeue -h -j "$ids" -o '%A') || return 1
    mapfile -t OUTSTANDING_IDS < <(printf '%s\n' "$output" | sed '/^$/d' | sort -u)
}
for batch in batches/batch_*.sh; do
    source "$batch"
    PARALLEL=${PARALLEL:-${#TASK_IDS[@]}}
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    # A first-wave placement mismatch may be explicitly requeued five times by batch.sh.
    # Append output so the log retains diagnostics from each placement attempt.
    OPTIONS=(--parsable --partition="$PARTITION" --job-name="$JOB_PREFIX" --requeue --open-mode=append
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
    while :; do
        if (( ${#OUTSTANDING_IDS[@]} >= MAX_OUTSTANDING )); then
            refresh_outstanding
        fi
        if (( ${#OUTSTANDING_IDS[@]} >= MAX_OUTSTANDING )); then
            echo "Submission waiting: ${#OUTSTANDING_IDS[@]} batches still pending or running; $submitted/$next_batch accepted"
            sleep 5
            continue
        fi
        if SUBMITTED=$(sbatch "${OPTIONS[@]}" batch.sh 2>&1); then break; fi
        if [[ "$SUBMITTED" == *AssocMaxSubmitJobLimit* || "$SUBMITTED" == *QOSMaxSubmitJobPerUserLimit* || "$SUBMITTED" == *QOSMaxSubmitJobPerAccountLimit* ]]; then
            echo "Submission waiting: Slurm's submitted-job limit is full; $submitted/$next_batch accepted"
            sleep 5
            continue
        fi
        printf '%s\n' "$SUBMITTED" >&2
        printf 'failed %s %s\n' "$submitted" "$next_batch" > submission.state
        exit 1
    done
    ID=${SUBMITTED%%;*}
    [[ "$ID" =~ ^[0-9]+$ ]] || { echo "Invalid sbatch ID: $SUBMITTED" >&2; exit 1; }
    csv_row "$ID" 0 1 "$(date +%s)" "$BATCH_ID" >> submission.csv
    OUTSTANDING_IDS+=("$ID")
    submitted=$((submitted + 1))
    printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
    echo "Batch $BATCH_ID: Slurm $ID ($submitted/$next_batch accepted)"
done
printf 'complete %s %s\n' "$submitted" "$next_batch" > submission.state
trap - EXIT HUP INT TERM
echo "Submission complete: $submitted/$next_batch batches accepted"
echo "Remote directory: $JOB_DIR"
