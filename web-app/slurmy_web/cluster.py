"""Small, read-only Slurm overview for the selected SSH host and partition."""

import re
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
if [[ -z "$node" ]]; then sinfo -h -o '%P' | sed 's/\*$//' | sort -u; fi
printf 'NODES\n'
if [[ -z "$node" ]]; then
    if [[ "$partition" == all ]]; then
        sinfo -h -N -o '%N|%T|%c|%C|%m'
    else
        sinfo -h -N -p "$partition" -o '%N|%T|%c|%C|%m'
    fi
fi
printf 'JOBS\n'
format='%i|%u|%j|%T|%D|%C|%M|%l|%N|%R|%Z|%k'
if [[ -n "$node" ]]; then
    if [[ "$partition" == all ]]; then
        jobs=$(squeue -a -h -w "$node" -o "$format")
    else
        jobs=$(squeue -a -h -p "$partition" -w "$node" -o "$format")
    fi
elif [[ "$partition" == all ]]; then
    jobs=$(squeue -a -h -o "$format")
else
    jobs=$(squeue -a -h -p "$partition" -o "$format")
fi
printf '%s\n' "$jobs"
printf 'IDENTITY\n%s\n' "$(id -un)"
printf 'HOSTS\n'
printf '%s\n' "$jobs" | awk -F'|' 'NF >= 12 && $9 != "(null)" && $9 != "" {print $9}' | sort -u | while IFS= read -r expression; do
    hosts=$(scontrol show hostnames "$expression" 2>/dev/null | paste -sd, -) || continue
    printf '%s|%s\n' "$expression" "$hosts"
done
printf 'METADATA\n'
# Only metadata from the connected account's own, direct Slurmy job folders is
# readable. Slurm exposes other users' queue entries, not their private files.
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
    sections = {'PARTITIONS': [], 'NODES': [], 'JOBS': [], 'IDENTITY': [], 'HOSTS': [], 'METADATA': []}
    section = None
    for line in output.splitlines():
        if line in sections:
            section = line
        elif section and line:
            sections[section].append(line)
    if section != 'METADATA':
        raise RuntimeError('Incomplete Slurm status response.')
    metadata = {}
    hostlists = dict(line.split('|', 1) for line in sections['HOSTS'] if '|' in line)
    for line in sections['METADATA']:
        fields = line.split('|')
        if len(fields) == 3 and fields[1].isdigit() and fields[2].isdigit():
            metadata[fields[0]] = dict(jobpairs=int(fields[1]), batches=int(fields[2]))
    nodes = []
    seen_nodes = set()
    for line in sections['NODES']:
        fields = line.split('|')
        if len(fields) != 5:
            continue
        name, state, cpus, split, memory = fields
        if name in seen_nodes:
            continue  # A node can appear in more than one partition.
        try:
            allocated, idle, other, total = map(int, split.split('/'))
        except ValueError:
            continue
        nodes.append(dict(name=name, state=state, cpus=int(cpus), allocated=allocated,
                          idle=idle, other=other, total=total, memory_mib=int(memory)))
        seen_nodes.add(name)
    jobs = []
    users = defaultdict(lambda: dict(jobs=0, running=0, pending=0, allocated_cpus=0))
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
        entry = dict(id=job_id, user=user, name=name, state=state, nodes=node_count,
                     cpus=cpu_count, elapsed=elapsed, limit=limit, nodelist=nodelist,
                     hostnames=hostlists.get(nodelist, '').split(',') if nodelist in hostlists else [],
                     reason=reason, slurmy=slurmy, slurmy_id=parts[-1] if slurmy else None)
        public = re.fullmatch(r'slurmy:v1:calls=(\d+):batches=(\d+):batch=(\d+)', comment)
        if public and slurmy:
            entry.update(jobpairs=int(public.group(1)), batches=int(public.group(2)),
                         batch_index=int(public.group(3)), metadata_source='Slurm comment')
        if slurmy and directory in metadata:
            entry.update(metadata[directory])
            entry['metadata_source'] = 'private job files'
        jobs.append(entry)
        account = users[user]
        account['jobs'] += 1
        if state.upper().startswith('RUNNING'):
            account['running'] += 1
            account['allocated_cpus'] += cpu_count
        elif state.upper().startswith('PENDING'):
            account['pending'] += 1
    allocated = sum(node['allocated'] for node in nodes)
    total = sum(node['total'] for node in nodes)
    return dict(host=host, partition=partition, user=sections['IDENTITY'][0] if sections['IDENTITY'] else '', updated=time.time(),
                partitions=sections['PARTITIONS'], nodes=nodes, jobs=jobs,
                users=[dict(name=name, **counts) for name, counts in sorted(users.items())],
                summary=dict(nodes=len(nodes), free_nodes=sum(n['state'].lower().startswith('idle') for n in nodes),
                             allocated_cpus=allocated, total_cpus=total,
                             utilization=round(100 * allocated / total, 1) if total else 0,
                             running=sum(j['state'].upper().startswith('RUNNING') for j in jobs),
                             pending=sum(j['state'].upper().startswith('PENDING') for j in jobs)))


def fetch_status(host, partition, timeout=12, node=None):
    if not PARTITION_RE.fullmatch(partition):
        raise ValueError('Invalid Slurm partition name.')
    if node is not None and not PARTITION_RE.fullmatch(node):
        raise ValueError('Invalid Slurm node name.')
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
