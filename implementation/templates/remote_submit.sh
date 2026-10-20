#!/usr/bin/env bash
# Run on the head node: validate scheduling policy and topology, then sbatch.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid Slurmy job ID.' >&2; exit 2; }
SLURMY_JOB_ID=$1
JOB_DIR="$HOME/slurmy/jobs/$SLURMY_JOB_ID"
export JOB_DIR
[[ -d "$JOB_DIR" && ! -L "$JOB_DIR" ]] || { echo 'Committed Slurmy job directory is missing.' >&2; exit 1; }
exec 9> "$JOB_DIR/.submit-controller.lock"
flock -n 9 || { echo 'Another controller is already submitting this Slurmy job.' >&2; exit 1; }
PARTITION=$2
JOB_PREFIX=$4
[[ "$JOB_PREFIX" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || {
    echo 'Invalid job prefix.' >&2; exit 1;
}
mkdir -p "$JOB_DIR/rootfs" "$JOB_DIR/results/index" "$JOB_DIR/progress" "$JOB_DIR/logs"
if [[ ! -f "$JOB_DIR/.inputs-ready" ]]; then
    : > "$JOB_DIR/results/index/.ready"
    tar -xzf "$JOB_DIR/incoming/job-files.tar.gz" -C "$JOB_DIR"
    tar -xzf "$JOB_DIR/incoming/inputs.tar.gz" -C "$JOB_DIR/rootfs"
else
    echo "Using committed inputs for $1; preserving existing results and inputs."
fi
cd "$JOB_DIR"
RESUME_INCOMPLETE=${6:-0}
RESERVATION=${7:--}
[[ "$RESERVATION" != - ]] || RESERVATION=
[[ -z "$RESERVATION" || "$RESERVATION" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || { echo 'Invalid reservation.' >&2; exit 2; }
if [[ ! -f submission_context.sh ]]; then
    printf 'SAVED_PARTITION=%q\nSAVED_MAX_OUTSTANDING=%q\nSAVED_JOB_PREFIX=%q\nSAVED_JOB_NAME_B64=%q\nSAVED_RESERVATION=%q\n' \
        "$PARTITION" "${3:-32}" "$JOB_PREFIX" "${5:-}" "$RESERVATION" > submission_context.sh
fi
if [[ -f .stopped ]]; then
    echo 'This job is stopped. Use Resume job before submitting more batches.' >&2
    exit 1
fi
[[ -f ./submit_coordinator.py ]] || {
    echo 'This job predates coordinated submission; resume it with its original prepared submit.sh.' >&2
    exit 1
}
if [[ ! -f submission.csv ]]; then python3 - "${5:-}" <<'PY'
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
fi
source ./csv.sh
TOTAL_CALLS=$(sed -n 's/.*"task_count": *\([0-9][0-9]*\).*/\1/p' metadata.json | head -n 1)
[[ "$TOTAL_CALLS" =~ ^[0-9]+$ ]] || { echo 'Missing call count in metadata.json.' >&2; exit 1; }

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
    unset TASK_THREADS_PER_CORE MAX_THREADS_PER_CORE
    source "$plan"
    MAX_THREADS_PER_CORE=${MAX_THREADS_PER_CORE:-1}
    (( MAX_THREADS_PER_CORE >= 1 && MAX_THREADS_PER_CORE <= THREADS_PER_CORE )) || {
        echo "A jobpair in $plan requests $MAX_THREADS_PER_CORE threads per core; partition $PARTITION supports $THREADS_PER_CORE." >&2; exit 1;
    }
    if ! declare -p TASK_THREADS_PER_CORE &>/dev/null; then
        TASK_THREADS_PER_CORE=()
        for ((i=0; i<${#TASK_IDS[@]}; i++)); do TASK_THREADS_PER_CORE+=(1); done
    fi
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
            printf 'TASK_THREADS_PER_CORE=('; printf ' %q' "${TASK_THREADS_PER_CORE[@]:first:count}"; printf ' )\n'
            printf 'BATCH_ID=%s\nPARALLEL=%s\nEXCLUSIVE_CPU=%s\nEXCLUSIVE_NODE=%s\nMAX_CORES=%s\nMAX_CPUS=%s\nMAX_THREADS_PER_CORE=%s\nMEMORY_MIB=%s\nWALL_SECONDS=%s\n' \
                "$next_batch" "$parallel" "$EXCLUSIVE_CPU" "$EXCLUSIVE_NODE" "$MAX_CORES" "$MAX_CPUS" "$MAX_THREADS_PER_CORE" "$((parallel * SLOT_MEMORY))" "$wall"
            printf 'RESERVED_CORES=%s\n' "$((parallel * SLOT_CORES))"
        } > "$file"
        next_batch=$((next_batch + 1))
    done
done
sed -i -E "s/\"batch_count\": [0-9]+/\"batch_count\": $next_batch/" metadata.json

# Validate every batch before submitting any of them.
for batch in batches/batch_*.sh; do
    source "$batch"
    MAX_THREADS_PER_CORE=${MAX_THREADS_PER_CORE:-1}
    (( MAX_THREADS_PER_CORE >= 1 && MAX_THREADS_PER_CORE <= THREADS_PER_CORE )) || {
        echo "Batch $BATCH_ID requests $MAX_THREADS_PER_CORE threads per core; partition $PARTITION supports $THREADS_PER_CORE." >&2; exit 1;
    }
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
if [[ -f submission.csv ]]; then
    prior_ids=$(python3 - <<'PY'
import csv
from pathlib import Path

with Path('submission.csv').open(newline='', encoding='utf-8') as stream:
    rows = list(csv.DictReader(stream))
seen = set()
for row in rows:
    batch = int(row['batch_id'])
    if not row['slurm_job_id'].isdigit() or (batch not in seen and batch != len(seen)):
        raise SystemExit('Cannot resume: submission.csv is not a contiguous batch prefix.')
    seen.add(batch)
for row in rows:
    if int(row['batch_id']) in seen:
        print(row['slurm_job_id'])
PY
    )
    OUTSTANDING_IDS=()
    if [[ -n "$prior_ids" ]]; then mapfile -t OUTSTANDING_IDS <<< "$prior_ids"; fi
    submitted=$(python3 - <<'PY'
import csv
with open('submission.csv', newline='') as stream:
    print(len({int(row['batch_id']) for row in csv.DictReader(stream)}))
PY
    )
else
    csv_row slurm_job_id offset array_size submitted_epoch batch_id > submission.csv
    OUTSTANDING_IDS=()
    submitted=0
fi
# Keep this controller connected until every batch is accepted. The per-job
# window is combined with the user's shared Slurmy capacity by the coordinator.
MAX_OUTSTANDING=${3:-32}
[[ "$MAX_OUTSTANDING" =~ ^[1-9][0-9]*$ ]] || {
    echo 'SLURMY_MAX_OUTSTANDING must be a positive integer.' >&2; exit 1;
}
printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
finish_submit() {
    local rc=$?
    python3 "$JOB_DIR/submit_coordinator.py" release "$SLURMY_JOB_ID" || true
    if (( rc != 0 )); then printf "failed %s %s\n" "$submitted" "$next_batch" > submission.state; fi
}
trap finish_submit EXIT
trap 'exit 1' HUP INT TERM
declare -A RESUME_WANTED=()
if (( RESUME_INCOMPLETE )); then
    mapfile -t RESUME_BATCHES < <(python3 - <<'PY'
import csv, json
from pathlib import Path
metadata = json.loads(Path('metadata.json').read_text())
done = set()
for path in Path('results').glob('batch_*_task_*.csv'):
    try:
        with path.open(newline='') as stream:
            row = next(csv.DictReader(stream))
        if row.get('complete', '').lower() == 'true':
            done.add(int(row['task_id']))
    except (OSError, ValueError, StopIteration):
        pass
for batch, tasks in enumerate(metadata['planned_batch_tasks']):
    if any(task not in done for task in tasks):
        print(batch)
PY
    )
    for batch_id in "${RESUME_BATCHES[@]}"; do RESUME_WANTED[$batch_id]=1; done
fi
echo "Slurmy job ID: $1"
echo "Submission progress: $submitted/$next_batch batches accepted (at most $MAX_OUTSTANDING outstanding)"
for batch in batches/batch_*.sh; do
    source "$batch"
    [[ ! -f .stopped ]] || { echo 'Submission stopped before the next batch.' >&2; exit 1; }
    if (( BATCH_ID < submitted )) && [[ ! ${RESUME_WANTED[$BATCH_ID]:-} ]]; then continue; fi
    PARALLEL=${PARALLEL:-${#TASK_IDS[@]}}
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    # A first-wave placement mismatch may be explicitly requeued five times by batch.sh.
    # Append output so the log retains diagnostics from each placement attempt.
    # Queue-visible metadata lets other Slurmy users see counts without access
    # to this job's private directory. It contains no local paths or commands.
    OPTIONS=(--parsable --partition="$PARTITION" --job-name="$JOB_PREFIX"
        --comment="slurmy:v1:calls=$TOTAL_CALLS:batches=$next_batch:batch=$BATCH_ID"
        --requeue --open-mode=append
        --nodes=1 --ntasks="$PARALLEL" --cpus-per-task="$((SLOT_CORES * MAX_THREADS_PER_CORE))"
        --threads-per-core="$MAX_THREADS_PER_CORE" --distribution=block:block
        --mem="${MEMORY_MIB}M" --time="$(( (WALL_SECONDS + 59) / 60 ))"
        --array=0-0 --export="ALL,SLURMY_BATCH_ID=$BATCH_ID" --output=logs/slurm_%A_%a.out
        --signal=B:TERM@30)
    if [[ -n "$RESERVATION" ]]; then OPTIONS+=(--reservation="$RESERVATION"); fi
    if (( EXCLUSIVE_CPU )); then
        # With CR_CORE, reject partially occupied sockets during selection.
        # A socket-sized core count alone can otherwise span two partial sockets.
        OPTIONS+=(--ntasks-per-socket=1 --cores-per-socket="$CORES_PER_SOCKET"
            --sockets-per-node="$((PARALLEL * MAX_CPUS))")
    fi
    if (( EXCLUSIVE_NODE )); then OPTIONS+=(--exclusive); fi
    while :; do
        [[ ! -f .stopped ]] || { echo 'Submission stopped before the next batch.' >&2; exit 1; }
        if ! SUBMITTED=$(python3 "$JOB_DIR/submit_coordinator.py" attempt "$SLURMY_JOB_ID" "$MAX_OUTSTANDING" -- "${OPTIONS[@]}" batch.sh); then
            printf '%s\n' "$SUBMITTED" >&2
            exit 1
        fi
        case "$SUBMITTED" in
            ACCEPTED\|*)
                echo "Submission capacity: ${SUBMITTED#*|*|}"
                break ;;
            WAIT\|*)
                echo "Submission waiting: ${SUBMITTED#WAIT|}; $submitted/$next_batch accepted"
                sleep 5 ;;
            *)
                printf 'Unexpected submit coordinator response: %s\n' "$SUBMITTED" >&2
                exit 1 ;;
        esac
    done
    ID=${SUBMITTED#ACCEPTED|}
    ID=${ID%%|*}
    [[ "$ID" =~ ^[0-9]+$ ]] || { echo "Invalid sbatch ID: $SUBMITTED" >&2; exit 1; }
    row=$(csv_row "$ID" "$BATCH_ID" 1 "$(date +%s)" "$BATCH_ID")
    printf '%s\n' "$row" >> submission.csv
    OUTSTANDING_IDS+=("$ID")
    if (( BATCH_ID >= submitted )); then submitted=$((submitted + 1)); fi
    printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
    echo "Batch $BATCH_ID: Slurm $ID ($submitted/$next_batch accepted)"
done
printf 'complete %s %s\n' "$submitted" "$next_batch" > submission.state
echo "Submission complete: $submitted/$next_batch batches accepted"
echo "Remote directory: $JOB_DIR"
