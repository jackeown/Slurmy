"""Live CPU-capacity estimate for the workflow builder's batch-size default."""
import subprocess

from slurmy import boolean, positive
from . import workflows


def partition_nodes(host, partition):
    host, partition = workflows.alias(host), workflows.alias(partition)
    try:
        result = subprocess.run(
            ['ssh', '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--', host,
             f"sinfo -p {partition} -N -h -o '%N|%X|%Y|%Z'"],
            text=True, capture_output=True, timeout=25, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError(f'Cannot inspect partition topology: {exc}') from exc
    if result.returncode:
        raise ValueError(result.stderr.strip() or 'Cannot inspect partition topology.')
    nodes = {}
    for line in result.stdout.splitlines():
        fields = line.strip().split('|')
        if len(fields) != 4 or any(not field.isdecimal() or int(field) < 1 for field in fields[1:]):
            raise ValueError('The cluster returned incomplete node topology.')
        nodes[fields[0]] = dict(sockets=int(fields[1]), cores_per_socket=int(fields[2]),
                                threads_per_core=int(fields[3]))
    if not nodes:
        raise ValueError('No compute nodes found in the selected partition.')
    return list(nodes.values())


def estimate(configurations, total_pairs, maximum_outstanding, nodes):
    if total_pairs < 1:
        raise ValueError('No solver-problem pairs match the selected sources.')
    capacities = []
    for configuration in configurations:
        cores = positive(str(configuration.get('cores', '')), 'solver cores')
        threads = positive(str(configuration.get('threads_per_core') or 1), 'threads per core')
        dedicated_node = boolean(str(configuration.get('exclusive_node') or ''), 'dedicated node')
        dedicated_socket = boolean(str(configuration.get('exclusive_cpu') or ''), 'dedicated CPU')
        slots = 0
        for node in nodes:
            sockets, per_socket = node['sockets'], node['cores_per_socket']
            if cores > sockets * per_socket or threads > node['threads_per_core']:
                continue
            if dedicated_node:
                slots += 1
            elif dedicated_socket:
                slots += sockets // ((cores + per_socket - 1) // per_socket)
            else:
                slots += sockets * per_socket // cores
        if not slots:
            name = configuration.get('solver_name') or 'A solver configuration'
            raise ValueError(f'{name} cannot run on any node in this partition.')
        capacities.append(slots)
    if not capacities:
        raise ValueError('Add at least one solver configuration.')
    hardware = min(capacities)
    parallelism = min(hardware, positive(str(maximum_outstanding), 'maximum outstanding SLURM tasks'))
    return dict(batch_size=max(1, (total_pairs + 2 * parallelism - 1) // (2 * parallelism)),
                total_pairs=total_pairs, node_count=len(nodes), hardware_parallelism=hardware,
                maximum_parallelism=parallelism, configuration_parallelism=capacities)


def suggest(data):
    host, partition = workflows.alias(data.get('host')), workflows.alias(data.get('partition'))
    mode = data.get('mode')
    if mode == 'jobpairs':
        configurations = workflows.imported_rows(workflows.existing(data.get('jobpairs')), check_paths=False)
        count = len(configurations)
    elif mode in {'interactive', 'configurations'}:
        configurations = (data.get('configurations') if mode == 'interactive' else
                          workflows.imported_rows(workflows.existing(data.get('configurations_file')), True, False))
        if not isinstance(configurations, list) or not configurations:
            raise ValueError('Add at least one solver configuration.')
        problems = set()
        for value in data.get('globs', []):
            _, matches = workflows.problem_glob(value, materialize=False, host=host)
            problems.update(matches)
        count = len(configurations) * len(problems)
    else:
        raise ValueError('Choose how to define solver-problem pairs.')
    return estimate(configurations, count, data.get('max_outstanding', 32), partition_nodes(host, partition))
