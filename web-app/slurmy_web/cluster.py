"""Small, read-only SLURM overview for the selected SSH host and partition."""

import re
import json
import math
import shutil
import subprocess
import time
from collections import defaultdict
from pathlib import PurePosixPath


PARTITION_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")
REMOTE_STATUS_SCRIPT = r'''set -euo pipefail
partition=$1
node=${2:-}
printf 'PARTITIONS\n'
if [[ -z "$node" ]]; then sinfo -a -h -o '%P' | sed 's/\*$//' | sort -u; fi
printf 'NODES\n'
if [[ -z "$node" ]]; then
    if [[ "$partition" == all ]]; then
        sinfo -a -h -N -o '%N|%T|%c|%C|%m|%Z'
    else
        sinfo -h -N -p "$partition" -o '%N|%T|%c|%C|%m|%Z'
    fi
fi
printf 'TOPOLOGY\n'
# Node details are also needed when only the jobs on one node are requested.
sinfo -a -h -N -o '%N|%Z|%Y' | sort -u
printf 'ALLOCATIONS\n'
allocations=$(scontrol -a show job -d) || exit 1
printf '%s\n' "$allocations"
printf 'JOBS\n'
format='%i|%u|%j|%T|%D|%C|%M|%l|%N|%R|%Z|%k'
if [[ -n "$node" ]]; then
    if [[ "$partition" == all ]]; then
        jobs=$(squeue -a -r -h -w "$node" -o "$format")
    else
        jobs=$(squeue -a -r -h -p "$partition" -w "$node" -o "$format")
    fi
elif [[ "$partition" == all ]]; then
    jobs=$(squeue -a -r -h -o "$format")
else
    jobs=$(squeue -a -r -h -p "$partition" -o "$format")
fi
printf '%s\n' "$jobs"
printf 'IDENTITY\n%s\n' "$(id -un)"
printf 'HOSTS\n'
{ printf '%s\n' "$jobs" | awk -F'|' 'NF >= 12 && $9 != "(null)" && $9 != "" {print $9}';
  printf '%s\n' "$allocations" | sed -n 's/.* Nodes=\([^ ]*\) CPU_IDs=.*/\1/p';
} | sort -u | while IFS= read -r expression; do
    hosts=$(scontrol show hostnames "$expression" 2>/dev/null | paste -sd, -) || continue
    printf '%s|%s\n' "$expression" "$hosts"
done
printf 'CALLS\n'
printf '%s\n' "$jobs" | while IFS='|' read -r slurm_id user name state nodes cpus elapsed limit nodelist reason directory comment; do
    [[ "$state" == RUNNING* && "$directory" == "$HOME"/slurmy/jobs/* ]] || continue
    job_name=${directory##*/}
    [[ "$directory" == "$HOME/slurmy/jobs/$job_name" ]] || continue
    [[ "$comment" =~ :batch=([0-9]+)$ ]] || continue
    batch=${BASH_REMATCH[1]}
    progress=$(printf '%s/progress/batch_%06d.csv' "$directory" "$batch")
    [[ -f "$progress" && -f "$directory/manifest.jsonl" ]] || continue
    IFS=, read -r call_state task_id _ < "$progress" || continue
    call_state=${call_state//\"/}; task_id=${task_id//\"/}
    [[ "$call_state" == running && "$task_id" =~ ^[0-9]+$ ]] || continue
    definition=$(sed -n "$((task_id + 1))p" "$directory/manifest.jsonl")
    [[ -n "$definition" ]] && printf '%s|%s|%s\n' "$slurm_id" "$task_id" "$definition"
done
printf 'METADATA\n'
# Only metadata from the connected account's own, direct Slurmy job folders is
# readable. SLURM exposes other users' queue entries, not their private files.
printf '%s\n' "$jobs" | awk -F'|' 'NF >= 12 {print $11}' | sort -u | while IFS= read -r directory; do
    case "$directory" in "$HOME"/slurmy/jobs/*) ;; *) continue ;; esac
    name=${directory##*/}
    [[ -n "$name" && "$directory" == "$HOME/slurmy/jobs/$name" ]] || continue
    file="$directory/metadata.json"
    [[ -f "$file" && -r "$file" && ! -L "$file" ]] || continue
    calls=$(sed -n 's/.*"task_count": *\([0-9][0-9]*\).*/\1/p' "$file" | head -n 1)
    batches=$(sed -n 's/.*"batch_count": *\([0-9][0-9]*\).*/\1/p' "$file" | head -n 1)
    [[ "$calls" =~ ^[0-9]+$ ]] || continue
    printf '%s|%s|%s\n' "$directory" "$calls" "${batches:-0}"
done
'''


def parse_status(host, partition, output):
    sections = {name: [] for name in ('PARTITIONS', 'NODES', 'TOPOLOGY', 'ALLOCATIONS',
                                     'JOBS', 'IDENTITY', 'HOSTS', 'CALLS', 'METADATA')}
    section = None
    for line in output.splitlines():
        if line in sections:
            section = line
        elif section and line:
            sections[section].append(line)
    if section != 'METADATA':
        raise RuntimeError('Incomplete SLURM status response.')
    metadata = {}
    active_calls = {}
    for line in sections['CALLS']:
        try:
            slurm_id, task_id, encoded = line.split('|', 2)
            call = json.loads(encoded)
            if int(task_id) != call.get('task_id'):
                continue
            active_calls[slurm_id] = dict(id=int(task_id), solver=str(call.get('solver') or call.get('solver_name') or call.get('system') or 'Solver'),
                                          problem=str(call.get('problem') or 'Problem'),
                                          cores=int(call['cores']) if str(call.get('cores', '')).isdigit() else None,
                                          isolation=('node' if call.get('exclusive_node') else 'socket' if call.get('exclusive_cpu') else 'shared')
                                          if 'exclusive_node' in call or 'exclusive_cpu' in call else None)
        except (ValueError, TypeError, json.JSONDecodeError):
            continue
    hostlists = dict(line.split('|', 1) for line in sections['HOSTS'] if '|' in line)
    topology = {}
    cores_per_socket = {}
    for line in sections['TOPOLOGY']:
        fields = line.split('|')
        if len(fields) < 2:
            continue
        name, value = fields[:2]
        if value.isdigit() and int(value) > 0:
            topology[name] = int(value)
        if len(fields) > 2 and fields[2].isdigit() and int(fields[2]) > 0:
            cores_per_socket[name] = int(fields[2])
    # SLURM's CPU_IDs use abstract topology order, with sibling CPUs adjacent.
    # Count distinct physical cores, not NumCPUs or the call's requested cores:
    # socket-exclusive jobs may reserve more cores than the solver uses.
    allocations = {}
    placements = {}
    job_ids = []
    for line in sections['ALLOCATIONS']:
        identity = re.search(r'\bJobId=(\S+)', line)
        if identity:
            job_ids = [identity[1]]
            array = re.search(r'\bArrayJobId=(\d+)\s+ArrayTaskId=(\d+)\b', line)
            if array:
                job_ids.append(f'{array[1]}_{array[2]}')
        placement = re.search(r'\bNodes=(\S+)\s+CPU_IDs=([\d,-]+)(?=\s|$)', line)
        if not placement:
            continue
        expression, cpu_ids = placement.groups()
        for node_name in hostlists.get(expression, expression).split(','):
            siblings = topology.get(node_name)
            if not siblings:
                continue
            cores = set()
            for interval in cpu_ids.split(','):
                first, _, last = interval.partition('-')
                cores.update(cpu // siblings for cpu in range(int(first), int(last or first) + 1))
            for job_id in job_ids:
                allocations.setdefault(job_id, {})[node_name] = len(cores)
                socket_size = cores_per_socket.get(node_name)
                placements.setdefault(job_id, {})[node_name] = dict(
                    core_ids=sorted(cores),
                    socket_ids=sorted({core // socket_size for core in cores}) if socket_size else None)
    for line in sections['METADATA']:
        fields = line.split('|')
        if len(fields) == 3 and fields[1].isdigit() and fields[2].isdigit():
            metadata[fields[0]] = dict(jobpairs=int(fields[1]), batches=int(fields[2]))
    nodes = []
    seen_nodes = set()
    for line in sections['NODES']:
        fields = line.split('|')
        if len(fields) != 6:
            continue
        name, state, cpus, split, memory, siblings = fields
        if name in seen_nodes:
            continue  # A node can appear in more than one partition.
        try:
            allocated, idle, other, total = map(int, split.split('/'))
            siblings = int(siblings)
            if siblings < 1:
                continue
        except ValueError:
            continue
        nodes.append(dict(name=name, state=state, cores=int(cpus) // siblings,
                          allocated=math.ceil(allocated / siblings), idle=idle // siblings,
                          other=math.ceil(other / siblings), total=total // siblings,
                          memory_mib=int(memory)))
        seen_nodes.add(name)
    jobs = []
    users = defaultdict(lambda: dict(jobs=0, running=0, pending=0, allocated_cores=0))
    for line in sections['JOBS']:
        fields = line.split('|', 11)
        if len(fields) != 12:
            continue
        job_id, user, name, state, node_count, cpus, elapsed, limit, nodelist, reason, directory, comment = fields
        try:
            cpu_count, node_count = int(cpus), int(node_count)
        except ValueError:
            continue
        # Slurmy submits from its private job directory. A different user's
        # metadata cannot be read; do not imply that missing counts mean zero.
        parts = PurePosixPath(directory).parts
        slurmy = len(parts) >= 5 and parts[-3:-1] == ('slurmy', 'jobs')
        node_cores = allocations.get(job_id, {})
        allocated_nodes = hostlists.get(nodelist, '').split(',') if nodelist in hostlists else []
        reserved_cores = (sum(node_cores.values()) if node_cores and
                          all(node in node_cores for node in allocated_nodes) else None)
        entry = dict(id=job_id, user=user, name=name, state=state, nodes=node_count,
                     reserved_cores=reserved_cores, node_cores=node_cores,
                     node_placement=placements.get(job_id, {}),
                     elapsed=elapsed, limit=limit, nodelist=nodelist,
                     hostnames=allocated_nodes,
                     reason=reason, slurmy=slurmy, slurmy_id=parts[-1] if slurmy else None,
                     active_call=active_calls.get(job_id) if slurmy else None,
                     solver_cores=active_calls.get(job_id, {}).get('cores') if slurmy else None,
                     solver_isolation=active_calls.get(job_id, {}).get('isolation') if slurmy else None)
        public = re.fullmatch(r'slurmy:v1:calls=(?P<calls>\d+):batches=(?P<batches>\d+)(?::cores=(?P<cores>\d+))?(?::isolation=(?P<isolation>shared|socket|node))?:batch=(?P<batch>\d+)', comment)
        if public and slurmy:
            entry.update(jobpairs=int(public.group('calls')), batches=int(public.group('batches')),
                         batch_index=int(public.group('batch')), metadata_source='SLURM comment')
            if public.group('cores'):
                entry['solver_cores'] = int(public.group('cores'))
            if public.group('isolation'):
                entry['solver_isolation'] = public.group('isolation')
        if slurmy and directory in metadata:
            entry.update(metadata[directory])
            entry['metadata_source'] = 'private job files'
        jobs.append(entry)
        account = users[user]
        account['jobs'] += 1
        if state.upper().startswith('RUNNING'):
            account['running'] += 1
            if reserved_cores is None:
                account['allocated_cores'] = None
            elif account['allocated_cores'] is not None:
                account['allocated_cores'] += reserved_cores
        elif state.upper().startswith('PENDING'):
            account['pending'] += 1
    allocated = sum(node['allocated'] for node in nodes)
    total = sum(node['total'] for node in nodes)
    return dict(host=host, partition=partition, user=sections['IDENTITY'][0] if sections['IDENTITY'] else '', updated=time.time(),
                partitions=sections['PARTITIONS'], nodes=nodes, jobs=jobs,
                users=[dict(name=name, **counts) for name, counts in sorted(users.items())],
                summary=dict(nodes=len(nodes), free_nodes=sum(n['state'].lower().startswith('idle') for n in nodes),
                             allocated_cores=allocated, total_cores=total,
                             utilization=round(100 * allocated / total, 1) if total else 0,
                             running=sum(j['state'].upper().startswith('RUNNING') for j in jobs),
                             pending=sum(j['state'].upper().startswith('PENDING') for j in jobs)))


def fetch_status(host, partition, timeout=12, node=None):
    if not PARTITION_RE.fullmatch(partition):
        raise ValueError('Invalid SLURM partition name.')
    if node is not None and not PARTITION_RE.fullmatch(node):
        raise ValueError('Invalid SLURM node name.')
    if shutil.which('ssh') is None:
        raise RuntimeError('ssh is not installed or not on PATH')
    command = ['ssh', '-T', '-o', 'BatchMode=yes', '-o', f'ConnectTimeout={timeout}',
               '--', host, 'bash', '-s', '--', partition, node or '']
    try:
        process = subprocess.run(command, input=REMOTE_STATUS_SCRIPT, text=True,
                                 capture_output=True, timeout=timeout + 15, check=False)
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError('Cluster status request timed out.') from exc
    except OSError as exc:
        raise RuntimeError(f'Could not start cluster status SSH request: {exc}') from exc
    if process.returncode:
        raise RuntimeError(process.stderr.strip() or f'SSH exited with status {process.returncode}')
    return parse_status(host, partition, process.stdout)
