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
SUBMISSION_PHASE='initializing committed inputs'
finish_submit() {
    local rc=$?
    if (( rc != 0 && rc != 75 )); then
        printf 'Submission controller failed during %s (exit %s; %s batches accepted). See preceding output for the cause.\n' "$SUBMISSION_PHASE" "$rc" "${submitted:-0}" >&2
    fi
    if [[ -f "$JOB_DIR/submit_coordinator.py" ]]; then python3 "$JOB_DIR/submit_coordinator.py" release "$SLURMY_JOB_ID" || true; fi
    if (( rc != 0 && rc != 75 )); then printf "failed %s %s\n" "${submitted:-0}" "${next_batch:-0}" > "$JOB_DIR/submission.state"; fi
}
trap finish_submit EXIT
trap 'exit 1' HUP INT TERM
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
ACCOUNT=${9:--}
[[ "$ACCOUNT" != - ]] || ACCOUNT=
[[ -z "$ACCOUNT" || "$ACCOUNT" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || { echo 'Invalid SLURM account.' >&2; exit 2; }
export SLURMY_ACCOUNT="$ACCOUNT"
RESERVATION=${7:--}
[[ "$RESERVATION" != - ]] || RESERVATION=
[[ -z "$RESERVATION" || "$RESERVATION" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || { echo 'Invalid reservation.' >&2; exit 2; }
if [[ -f submission_context.sh ]]; then
    source ./submission_context.sh
    # All waves of a committed job keep their original placement settings,
    # including an explicitly empty reservation. Later workflow edits only
    # affect newly dispatched jobs.
    PARTITION=${SAVED_PARTITION:-$PARTITION}
    RESERVATION=${SAVED_RESERVATION-}
    ACCOUNT=${SAVED_ACCOUNT-}
    export SLURMY_ACCOUNT="$ACCOUNT"
else
    printf 'SAVED_PARTITION=%q\nSAVED_MAX_OUTSTANDING=%q\nSAVED_JOB_PREFIX=%q\nSAVED_JOB_NAME_B64=%q\nSAVED_RESERVATION=%q\nSAVED_ACCOUNT=%q\n' \
        "$PARTITION" "${3:-32}" "$JOB_PREFIX" "${5:-}" "$RESERVATION" "$ACCOUNT" > submission_context.sh
fi
[[ "$PARTITION" =~ ^[A-Za-z0-9_.-]+$ ]] || { echo 'Invalid saved partition.' >&2; exit 2; }
[[ -z "$RESERVATION" || "$RESERVATION" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || { echo 'Invalid saved reservation.' >&2; exit 2; }
[[ -z "$ACCOUNT" || "$ACCOUNT" =~ ^[A-Za-z0-9][A-Za-z0-9_.-]*$ ]] || { echo 'Invalid saved account.' >&2; exit 2; }
printf 'SLURM settings: partition=%s; account=%s; reservation=%s\n' "$PARTITION" "${ACCOUNT:-cluster default}" "${RESERVATION:-none}"
if [[ -f .stopped ]]; then
    echo 'This job is stopped. Use Resume job before submitting more batches.' >&2
    exit 1
fi
[[ -f ./submit_coordinator.py ]] || {
    echo 'This job predates coordinated submission; resume it with its original prepared submit.sh.' >&2
    exit 1
}
if [[ ! -f submission.csv ]]; then
    # Serialize name assignment across jobs, not just this submission controller.
    exec 8> "$HOME/slurmy/.job-names.lock"
    flock 8
    python3 - "${5:-}" "${8:-0}" <<'PY'
import base64
import json
from pathlib import Path
import sys

name = base64.b64decode(sys.argv[1], validate=True).decode('utf-8')
if not name.strip() or len(name) > 100 or any(ord(char) < 32 or ord(char) == 127 for char in name):
    raise SystemExit('Invalid job name.')
path = Path('metadata.json')
metadata = json.loads(path.read_text(encoding='utf-8'))
if metadata.get('job_id') == Path.cwd().name and metadata.get('job_name'):
    name = metadata['job_name']  # Retrying submission keeps its assigned name.
elif sys.argv[2] == '1':
    from job_names import unused_job_name
    existing = set()
    for directory in Path.cwd().parent.iterdir():
        if directory == Path.cwd() or not directory.is_dir() or directory.is_symlink() or directory.name.startswith('.'):
            continue
        try:
            other = json.loads((directory / 'metadata.json').read_text(encoding='utf-8'))
        except (OSError, ValueError):
            # Incomplete and historical folders may not contain metadata yet.
            existing.add(directory.name.rsplit('_', 2)[0])
            continue
        existing.add(other.get('job_name') or Path(other.get('workflow_directory') or directory.name).name)
    name = unused_job_name(name, existing)
metadata['job_name'] = name.strip()
metadata['job_id'] = Path.cwd().name
temporary = path.with_suffix('.tmp')
temporary.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
temporary.replace(path)
print('Job name: ' + metadata['job_name'])
PY
    flock -u 8
    exec 8>&-
    if [[ -f ./prepare_axioms.sh ]]; then
        SUBMISSION_PHASE='preparing custom TPTP axioms'
        bash ./prepare_axioms.sh
    fi
fi
SUBMISSION_PHASE='reading solver-problem pair metadata'
source ./csv.sh
TOTAL_CALLS=$(sed -n 's/.*"task_count": *\([0-9][0-9]*\).*/\1/p' metadata.json | head -n 1)
[[ "$TOTAL_CALLS" =~ ^[0-9]+$ ]] || { echo 'Missing call count in metadata.json.' >&2; exit 1; }

# Physical core isolation requires allocation at core granularity and cgroups.
SUBMISSION_PHASE='checking SLURM configuration and partition topology'
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
SUBMISSION_PHASE='planning and validating batch allocations'

# Resolve automatic socket needs using the cluster's actual node topology.
mkdir -p batches
if [[ ! -f .batch-plans-ready ]] || ! grep -q '[0-9]' allocations.csv 2>/dev/null; then
csv_row batch_id concurrent_calls reserved_cores memory_mib wall_seconds exclusive_cpu exclusive_node > allocations.csv
next_batch=0
for plan in plans/batch_*.sh; do
    unset TASK_THREADS_PER_CORE MAX_THREADS_PER_CORE
    source "$plan"
    MAX_THREADS_PER_CORE=${MAX_THREADS_PER_CORE:-1}
    (( MAX_THREADS_PER_CORE >= 1 && MAX_THREADS_PER_CORE <= THREADS_PER_CORE )) || {
        echo "A solver-problem pair in $plan requests $MAX_THREADS_PER_CORE threads per core; partition $PARTITION supports $THREADS_PER_CORE." >&2; exit 1;
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
    # No per-pair memory bound: allocate one quarter of node RAM to this batch.
    if (( SLOT_MEMORY == 0 )); then
        SLOT_MEMORY=$((NODE_MEMORY / 4))
        (( SLOT_MEMORY > 0 )) || { echo 'Cannot determine quarter-node memory allocation.' >&2; exit 1; }
        for task_memory in "${TASK_MEMORY[@]}"; do
            (( task_memory == 0 || task_memory + 128 <= SLOT_MEMORY )) || {
                echo "A finite memory limit in $plan exceeds the quarter-node batch allocation. Separate these solver configurations or lower that limit." >&2; exit 1;
            }
        done
        echo "Batch $next_batch: no per-pair memory limit; allocating $SLOT_MEMORY MiB (one quarter of node memory)."
    fi
    memory_capacity=$((NODE_MEMORY / SLOT_MEMORY))
    (( capacity <= memory_capacity )) || capacity=$memory_capacity
    if (( EXCLUSIVE_CPU )); then
        socket_capacity=$((SOCKETS / MAX_CPUS))
        (( capacity <= socket_capacity )) || capacity=$socket_capacity
    fi
    (( capacity > 0 && MAX_CPUS <= SOCKETS && MAX_CORES <= MAX_CPUS * CORES_PER_SOCKET )) || {
        echo "A solver-problem pair in $plan cannot fit this partition's nodes." >&2; exit 1;
    }
    # A SLURM task owns the full planned batch; only PARALLEL calls run at once.
    for ((first=0; first<${#TASK_IDS[@]}; first+=${#TASK_IDS[@]})); do
        count=${#TASK_IDS[@]}
        parallel=1
        file=$(printf 'batches/batch_%06d.sh' "$next_batch")
        # Preserve the generated wall budget, including optional host-info collection.
        wall=$WALL_SECONDS
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
touch .batch-plans-ready
else
    next_batch=$(sed -n 's/.*"batch_count": *\([0-9][0-9]*\).*/\1/p' metadata.json | head -n 1)
    [[ "$next_batch" =~ ^[0-9]+$ ]] || exit 1
fi

if [[ -f submission.csv ]]; then
    SUBMISSION_PHASE='reading previously accepted batches'
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
MAX_OUTSTANDING=${3:-50}
[[ "$MAX_OUTSTANDING" =~ ^[1-9][0-9]*$ ]] || {
    echo 'SLURMY_MAX_OUTSTANDING must be a positive integer.' >&2; exit 1;
}
printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
declare -A RESUME_WANTED=()
SUBMISSION_PHASE='selecting unfinished solver-problem pairs'
if (( RESUME_INCOMPLETE )); then
    if (( RESUME_INCOMPLETE == 1 )); then
    resume_batches=$(python3 - <<'PY'
import csv, json
from pathlib import Path
metadata = json.loads(Path('metadata.json').read_text())
done = set()
results = Path('results')
# The compact indexes contain the same rows, including interrupted results.
# Reopening one NFS file per completed pair makes large resumes look stalled.
paths = (list((results / 'index-legacy').glob('batch_*.csv')) +
         list((results / 'index').glob('batch_*.csv')) if (results / 'index/.ready').is_file()
         else results.glob('batch_*_task_*.csv'))
for path in paths:
    with path.open(newline='') as stream:
        for row in csv.DictReader(stream):
            if row.get('complete', '').lower() == 'true':
                done.add(int(row['task_id']))
for batch, tasks in enumerate(metadata['planned_batch_tasks']):
    if any(task not in done for task in tasks):
        print(batch)
PY
    )
    printf '%s\n' "$resume_batches" > .resume-pending
    else
        resume_batches=$(cat .resume-pending 2>/dev/null || true)
    fi
    RESUME_BATCHES=()
    if [[ -n "$resume_batches" ]]; then mapfile -t RESUME_BATCHES <<< "$resume_batches"; fi
    for batch_id in "${RESUME_BATCHES[@]}"; do RESUME_WANTED[$batch_id]=1; done
fi
echo "Slurmy job ID: $1"
echo "Submission progress: $submitted/$next_batch batches accepted (at most $MAX_OUTSTANDING submitted)"
# Only batch arrays with exactly matching scheduling options. Local array indices
# map to stable Slurmy batch IDs through SLURMY_BATCH_OFFSET, so even workflows
# with more batches than MaxArraySize remain valid.
MAX_ARRAY_TASKS=$(python3 - "$CONFIG" <<'PY'
import re,sys
config=sys.argv[1]
size=re.search(r'^\s*MaxArraySize\s*=\s*(\d+)',config,re.M)
limit=int(size[1]) if size else 1
scheduler=re.search(r'\bmax_array_tasks=(\d+)',config)
if scheduler:limit=min(limit,int(scheduler[1]))
print(max(1,min(limit,1000)))
PY
)
# Mixed-version helpers must fall back to single-element submissions, never
# accept a multi-array with an old coordinator that only counts one slot.
if [[ $(python3 "$JOB_DIR/submit_coordinator.py" capabilities 2>/dev/null || true) != slurmy-arrays-v1 ]]; then
    MAX_ARRAY_TASKS=1
fi
ARRAY_MAPPING=offset
if ! grep -q 'SLURMY_BATCH_OFFSET' batch.sh; then MAX_ARRAY_TASKS=1; ARRAY_MAPPING=legacy; fi
# Add the element ID column without rewriting legacy offsets or identities.
python3 - <<'PY'
import csv,os
from pathlib import Path
path=Path('submission.csv')
with path.open(newline='') as stream:
    reader=csv.DictReader(stream);fields=reader.fieldnames;rows=list(reader)
if 'array_task_id' not in fields:
    temporary=path.with_suffix('.csv.tmp')
    with temporary.open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=[*fields,'array_task_id'])
        writer.writeheader();writer.writerows(rows)
    os.replace(temporary,path)
PY
GROUP_IDS=()
GROUP_OPTIONS=()
GROUP_SIGNATURE=
GROUP_PUBLIC=
submit_group() {
    local remaining first count accepted id details row_batch index
    local -a request_options
    while (( ${#GROUP_IDS[@]} )); do
        remaining=${#GROUP_IDS[@]}
        first=${GROUP_IDS[0]}
        request_options=("${GROUP_OPTIONS[@]}" --array="0-$((remaining-1))")
        if [[ "$ARRAY_MAPPING" == legacy ]]; then
            request_options+=(--export="ALL,SLURMY_BATCH_ID=$first")
        else
            request_options+=(--export="ALL,SLURMY_BATCH_OFFSET=$first")
        fi
        if (( remaining == 1 )); then
            request_options+=(--comment="slurmy:v1:$GROUP_PUBLIC:batch=$first")
        else
            request_options+=(--comment="slurmy:v2:$GROUP_PUBLIC:batch_offset=$first")
        fi
        while :; do
            [[ ! -f .stopped ]] || { echo 'Submission stopped before the next array.' >&2; exit 1; }
            if [[ ${SLURMY_MANAGED_CONTROLLER:-0} == 1 ]] && (( $(date +%s) >= ${SLURMY_CONTROLLER_DEADLINE:-0} )); then
                echo 'Submission waiting: yielding controller before its time limit'
                exit 75
            fi
            if ! SUBMITTED=$(python3 "$JOB_DIR/submit_coordinator.py" attempt "$SLURMY_JOB_ID" "$MAX_OUTSTANDING" -- "${request_options[@]}" batch.sh); then
                printf '%s\n' "$SUBMITTED" >&2
                exit 1
            fi
            case "$SUBMITTED" in
                ACCEPTED\|*) break ;;
                WAIT\|*)
                    echo "Submission waiting: ${SUBMITTED#WAIT|}; $submitted/$next_batch accepted"
                    if [[ ${SLURMY_MANAGED_CONTROLLER:-0} == 1 ]]; then exit 75; fi
                    sleep 5 ;;
                *) printf 'Unexpected submit coordinator response: %s\n' "$SUBMITTED" >&2; exit 1 ;;
            esac
        done
        id=${SUBMITTED#ACCEPTED|}; id=${id%%|*}
        details=${SUBMITTED#*|*|}
        # Old coordinators accepted one element and did not report a count.
        accepted=1
        if [[ "$details" =~ ^accepted=([0-9]+)\; ]]; then
            accepted=${BASH_REMATCH[1]}
        elif (( remaining > 1 )); then
            echo "Cannot confirm the accepted array size: $SUBMITTED. Inspect SLURM before retrying." >&2; exit 1
        fi
        [[ "$id" =~ ^[0-9]+$ ]] && (( accepted >= 1 && accepted <= remaining )) || { echo "Invalid array acceptance: $SUBMITTED" >&2; exit 1; }
        echo "Submission capacity: $details"
        python3 - "$id" "$first" "$accepted" <<'PY'
import csv,os,sys,time
from pathlib import Path
identifier,first,count=sys.argv[1],int(sys.argv[2]),int(sys.argv[3])
with Path('submission.csv').open(newline='') as stream:fields=next(csv.reader(stream))
with Path('submission.csv').open('a',newline='') as stream:
    writer=csv.DictWriter(stream,fieldnames=fields)
    for element in range(count):
        writer.writerow(dict(slurm_job_id=identifier,offset=first,array_size=1,
                             submitted_epoch=int(time.time()),batch_id=first+element,array_task_id=element))
    stream.flush();os.fsync(stream.fileno())
pending=Path('.resume-pending')
if pending.exists():
    accepted=set(range(first,first+count))
    pending.write_text(''.join(line+'\n' for line in pending.read_text().splitlines() if line and int(line) not in accepted))
PY
        for ((index=0; index<accepted; index++)); do
            row_batch=$((first+index))
            if (( row_batch >= submitted )); then submitted=$((submitted+1)); fi
            echo "Batch $row_batch: SLURM ${id}_$index ($submitted/$next_batch accepted)"
        done
        OUTSTANDING_IDS+=("$id")
        printf 'submitting %s %s\n' "$submitted" "$next_batch" > submission.state
        GROUP_IDS=("${GROUP_IDS[@]:accepted}")
    done
}
for batch in batches/batch_*.sh; do
    SUBMISSION_PHASE="submitting ${batch##*/}"
    source "$batch"
    [[ ! -f .stopped ]] || { echo 'Submission stopped before the next batch.' >&2; exit 1; }
    if (( BATCH_ID < submitted )) && [[ ! ${RESUME_WANTED[$BATCH_ID]:-} ]]; then continue; fi
    PARALLEL=${PARALLEL:-${#TASK_IDS[@]}}
    SLOT_CORES=$MAX_CORES
    if (( EXCLUSIVE_CPU )); then SLOT_CORES=$((MAX_CPUS * CORES_PER_SOCKET)); fi
    ISOLATION=shared
    if (( EXCLUSIVE_CPU )); then ISOLATION=socket; fi
    if (( EXCLUSIVE_NODE )); then ISOLATION=node; fi
    OPTIONS=(--parsable --partition="$PARTITION" --job-name="$JOB_PREFIX"
        --requeue --open-mode=append
        --nodes=1 --ntasks="$PARALLEL" --cpus-per-task="$((SLOT_CORES * MAX_THREADS_PER_CORE))"
        --threads-per-core="$MAX_THREADS_PER_CORE" --distribution=block:block
        --mem="${MEMORY_MIB}M" --time="$(( (WALL_SECONDS + 59) / 60 ))"
        --output=logs/slurm_%A_%a.out --signal=B:TERM@30)
    if [[ -n "$RESERVATION" ]]; then OPTIONS+=(--reservation="$RESERVATION"); fi
    if [[ -n "$ACCOUNT" ]]; then OPTIONS+=(--account="$ACCOUNT"); fi
    if (( EXCLUSIVE_CPU )); then
        OPTIONS+=(--ntasks-per-socket=1 --cores-per-socket="$CORES_PER_SOCKET"
            --sockets-per-node="$((PARALLEL * MAX_CPUS))")
    fi
    if (( EXCLUSIVE_NODE )); then OPTIONS+=(--exclusive); fi
    public="calls=$TOTAL_CALLS:batches=$next_batch:cores=$MAX_CORES:isolation=$ISOLATION"
    printf -v signature '%q ' "${OPTIONS[@]}" "$public"
    if (( ${#GROUP_IDS[@]} )) && { [[ "$signature" != "$GROUP_SIGNATURE" ]] || (( BATCH_ID != GROUP_IDS[${#GROUP_IDS[@]}-1]+1 || ${#GROUP_IDS[@]} >= MAX_ARRAY_TASKS )); }; then
        submit_group
    fi
    if (( ${#GROUP_IDS[@]} == 0 )); then
        GROUP_OPTIONS=("${OPTIONS[@]}"); GROUP_SIGNATURE=$signature; GROUP_PUBLIC=$public
    fi
    GROUP_IDS+=("$BATCH_ID")
done
submit_group
printf 'complete %s %s\n' "$submitted" "$next_batch" > submission.state
echo "Submission complete: $submitted/$next_batch batches accepted"
echo "Remote directory: $JOB_DIR"
