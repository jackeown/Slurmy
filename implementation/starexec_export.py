"""Convert a compact Slurmy download to StarExec's info/output ZIP layout.

SZS mnemonics: https://szs.tptp.org/ and https://tptp-manual.tptp.org/ .
The CSV/layout are matched to the user's Job7575_info.zip and output.zip.
"""
from __future__ import annotations

from contextlib import ExitStack
import csv
from decimal import Decimal, InvalidOperation
import hashlib
import io
from pathlib import Path, PurePosixPath
import re
import tarfile
import tempfile
import zipfile

FIELDS = ('benchmark', 'solver', 'configuration', 'status', 'cpu time', 'wallclock time',
          'memory usage', 'result', 'expected', 'SZSResult', 'SZSOutput', 'SZSStatus')
STATUS = dict(theorem='THM', unsatisfiable='UNS', satisfiable='SAT', countersatisfiable='CSA',
              contradictoryaxioms='CAX', finitelysatisfiable='FSA', finitelyunsatisfiable='FUN',
              finitetheorem='FTH', equivalent='EQV', tautologousconclusion='TAC', timeout='TMO',
              gaveup='GUP', error='ERR', unknown='UNK', inappropriate='IAP', memoryout='MMO',
              resourceout='RSO', incomplete='INC')
OUTPUT = dict(proof='Prf', refutation='Ref', cnfrefutation='CRf', model='Mod', finitemodel='FMo',
              infinitemodel='IMo', saturation='Sat', assurance='Ass', none='Non', definitions='Ass')
PAIR_ID = re.compile(r'___([0-9]+)(?:\.stderr)?\.log$')
TIMESTAMP = re.compile(rb'^\d+(?:\.\d+)?/\d+(?:\.\d+)?\t')


def component(value):
    value = str(value).replace('%', '%25').replace('/', '%2F').replace('\\', '%5C').replace('\x00', '%00')
    return '%2E' + value[1:] if value in {'.', '..'} else value or 'unknown'


def job_number(job_id):
    # Stable numeric alias, also exactly representable by JavaScript consumers.
    return int.from_bytes(hashlib.sha256(job_id.encode()).digest()[:7], 'big') >> 3


def benchmark_paths(rows):
    paths, owners = {}, {}
    for row in rows:
        original = row['problem']
        if original in paths: continue
        if '/Problems/' in original:
            relative = 'Problems/' + original.split('/Problems/', 1)[1]
        else:
            relative = 'Problems/local/' + PurePosixPath(original).name
        parts = PurePosixPath(relative).parts
        relative = '/'.join(component(part) for part in parts if part != '/')
        if relative in owners and owners[relative] != original:
            path = PurePosixPath(relative)
            relative = str(path.parent / hashlib.sha256(original.encode()).hexdigest()[:16] / path.name)
        paths[original] = relative; owners[relative] = original
    return paths


def number(value, multiplier=1):
    try:
        result = Decimal(str(value)) * multiplier
        return str(result) if result.is_finite() and result >= 0 else '0.0'
    except (InvalidOperation, ValueError):
        return '0.0'


def summary_row(row, benchmark):
    complete = row.get('complete', '').lower() == 'true'
    result = row.get('szs_status') or '--'
    if not complete:
        state, mnemonic, output = 'incomplete', '-', '-'
    else:
        state = {'time-limit': 'timeout (cpu)' if Decimal(number(row.get('cpu_seconds'))) >= Decimal(number(row.get('cpu_limit'))) > 0 else 'timeout (wallclock)',
                 'memory-limit': 'memout', 'solver-error': 'error', 'resource-limiter-error': 'error', 'worker-error': 'error', 'interrupted': 'incomplete'}.get(row.get('status'), 'complete')
        mnemonic = STATUS.get(result.casefold(), 'UNK')
        output = OUTPUT.get(row.get('szs_output', '').casefold(), 'Non')
    solver = row.get('solver') or 'solver'
    return dict(zip(FIELDS, (benchmark, solver, solver, state,
                number(row.get('cpu_seconds')), number(row.get('wall_seconds')),
                number(row.get('max_memory_kib'), 1024), result,
                row.get('expected') or '--', f'{mnemonic}-{output}' if complete else '-', output, mnemonic)))


def streams(archive):
    for member in archive:
        if not member.isfile(): continue
        match = PAIR_ID.search(member.name)
        if match:
            yield int(match[1]), archive.extractfile(member)


def copy_output(source, output, prefix):
    # Existing line timestamps are retained; otherwise pair totals are labelled
    # explicitly so they cannot be mistaken for measured per-line timestamps.
    for line in source:
        output.write(line if TIMESTAMP.match(line) else prefix + line)
        if not line.endswith(b'\n'): output.write(b'\n')


def export(compact: Path, destination: Path, job_id: str):
    summaries = list(compact.glob('*_results-summary.csv'))
    if len(summaries) != 1: raise ValueError('Expected one downloaded results summary.')
    with summaries[0].open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    ids = [int(row['task_id']) for row in rows]
    if len(set(ids)) != len(ids): raise ValueError('Duplicate solver-problem pair IDs in summary.')
    rows.sort(key=lambda row: int(row['task_id']))
    benchmarks = benchmark_paths(rows)
    name = f'Job{job_number(job_id)}'
    names = (name + '_info.zip', name + '_output.zip')
    destination.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink(): raise ValueError('StarExec output directory cannot be a symbolic link.')
    with tempfile.TemporaryDirectory(prefix='.starexec-', dir=destination) as folder:
        stage = Path(folder)
        with zipfile.ZipFile(stage / names[0], 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as archive:
            with archive.open(f'{name}/{name}_info.csv', 'w', force_zip64=True) as member:
                with io.TextIOWrapper(member, encoding='utf-8', newline='') as text:
                    writer = csv.DictWriter(text, fieldnames=FIELDS); writer.writeheader()
                    writer.writerows(summary_row(row, benchmarks[row['problem']]) for row in rows)
        with ExitStack() as stack:
            archives = []
            for suffix in ('solver-stdout.tgz', 'solver-stderr.tgz'):
                paths = list(compact.glob('*_' + suffix))
                if len(paths) != 1: raise ValueError('Expected downloaded stdout and stderr archives.')
                archive = stack.enter_context(tarfile.open(paths[0], 'r|gz'))
                archives.append(iter(streams(archive)))
            current = [next(iterator, None) for iterator in archives]
            output_zip = stack.enter_context(zipfile.ZipFile(stage / names[1], 'w', zipfile.ZIP_DEFLATED, allowZip64=True))
            for row in rows:
                task_id = int(row['task_id'])
                for i, iterator in enumerate(archives):
                    while current[i] is not None and current[i][0] < task_id:
                        current[i] = next(iterator, None)
                present = [i for i in range(2) if current[i] is not None and current[i][0] == task_id]
                if not present: continue
                benchmark = PurePosixPath(benchmarks[row['problem']])
                solver = component(row.get('solver') or 'solver')
                path = f'{name}_output/{benchmark.parent}/{solver}___{solver}/{benchmark.name}/{task_id}.txt'
                prefix = f"{number(row.get('cpu_seconds'))}/{number(row.get('wall_seconds'))}\t".encode()
                with output_zip.open(path, 'w', force_zip64=True) as output:
                    output.write(prefix + b'% Slurmy export: un-timestamped lines use final pair totals, not per-line measurements.\n')
                    for i in present:
                        if i == 1: output.write(prefix + b'% Solver stderr follows stdout; original interleaving is unavailable.\n')
                        copy_output(current[i][1], output, prefix)
                        current[i] = next(archives[i], None)
                    output.write(prefix + b'EOF\n')
        # Both archives must be complete before replacing any previous download.
        for filename in names:
            (stage / filename).replace(destination / filename)
    return names, sum(row.get('complete', '').lower() == 'true' for row in rows), len(rows)
