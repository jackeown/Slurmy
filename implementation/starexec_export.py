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
import szs

FIELDS = ('benchmark', 'solver', 'configuration', 'status', 'cpu time', 'wallclock time',
          'memory usage', 'result', 'expected', 'SZSResult', 'SZSOutput', 'SZSStatus')
STATUS = {alias.casefold():code for name,code in szs.STATUSES.items() for alias in (name,code)}
OUTPUT = {alias.casefold():code for name,code in szs.DATA_FORMS.items() for alias in (name,code)}
PAIR_ID = re.compile(r'___([0-9]+)(?:\.stderr)?\.log$')
TIMESTAMP = re.compile(rb'^\d+(?:\.\d+)?/\d+(?:\.\d+)?[ \t]+')


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
    row = szs.normalize_row(row)
    complete = row.get('complete', '').lower() == 'true'
    result = row.get('szs_status') or '--'
    if not complete:
        state, mnemonic, output = 'incomplete', '-', '-'
    else:
        state = {'time-limit': 'timeout (cpu)' if result=='CPUTimeout' else 'timeout (wallclock)' if result=='WCTimeout' else 'timeout (cpu)' if Decimal(number(row.get('cpu_seconds'))) >= Decimal(number(row.get('cpu_limit'))) > 0 else 'timeout (wallclock)',
                 'memory-limit': 'memout', 'solver-error': 'error', 'resource-limiter-error': 'error', 'worker-error': 'error', 'interrupted': 'incomplete'}.get((row.get('execution_status') or row.get('status')), 'complete')
        mnemonic = szs.mnemonic(szs.base(result)) or szs.mnemonic({'time-limit':'Timeout', 'memory-limit':'MemoryOut', 'resource-limit':'ResourceOut', 'solver-error':'Error', 'resource-limiter-error':'Error'}.get((row.get('execution_status') or row.get('status')))) or 'UNK'
        output = szs.data_mnemonic(row.get('szs_output')) or 'Non'
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


def copy_output(source, output, prefix, defer_eof=False):
    # Keep native per-line measurements verbatim. Older untimestamped logs use
    # pair totals as a compatibility fallback, documented outside solver output.
    pending = None
    for line in source:
        if pending is not None:
            output.write(pending if TIMESTAMP.match(pending) else prefix + pending)
            if not pending.endswith(b'\n'): output.write(b'\n')
        pending = line
    if pending is not None:
        if defer_eof and TIMESTAMP.match(pending) and TIMESTAMP.sub(b'', pending, count=1).strip() == b'EOF':
            return pending if pending.endswith(b'\n') else pending+b'\n'
        output.write(pending if TIMESTAMP.match(pending) else prefix + pending)
        if not pending.endswith(b'\n'): output.write(b'\n')
    return None


def export(compact: Path, destination: Path, job_id: str, *, csv_suffix: str = ""):
    summaries = list(compact.glob('*_results-summary.csv'))
    if len(summaries) != 1: raise ValueError('Expected one downloaded results summary.')
    with summaries[0].open(newline='', encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    ids = [int(row['task_id']) for row in rows]
    if len(set(ids)) != len(ids): raise ValueError('Duplicate solver-problem pair IDs in summary.')
    rows.sort(key=lambda row: int(row['task_id']))
    legacy = sum(row.get('has_combined_solver_output') != 'true' and
                 row.get('has_solver_stderr') == 'true' for row in rows)
    if legacy:
        print(f'Warning: {legacy} solver-problem pairs have legacy separate logs; '
              'original stderr timing/order was not recorded. Untimestamped lines '
              'use final pair totals in the StarExec compatibility fallback.', flush=True)
    benchmarks = benchmark_paths(rows)
    name = f'Job{job_number(job_id)}'
    names = (name + '_info.zip', name + '_output.zip')
    destination.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink(): raise ValueError('StarExec output directory cannot be a symbolic link.')
    with tempfile.TemporaryDirectory(prefix='.starexec-', dir=destination) as folder:
        stage = Path(folder)
        with zipfile.ZipFile(stage / names[0], 'w', zipfile.ZIP_DEFLATED, allowZip64=True) as archive:
            with archive.open(f'{name}/{name}_info{csv_suffix}.csv', 'w', force_zip64=True) as member:
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
                    native_eof = None
                    combined = row.get('has_combined_solver_output') == 'true'
                    if combined and 0 not in present:
                        raise ValueError(f'Missing combined output for solver-problem pair {task_id}.')
                    for i in present:
                        if combined and i == 1:
                            # Already present, genuinely timestamped, in solver-data.
                            current[i] = next(archives[i], None)
                            continue
                        eof = copy_output(current[i][1], output, prefix, defer_eof=(i == 0))
                        if eof is not None: native_eof = eof
                        current[i] = next(archives[i], None)
                    if native_eof is not None: output.write(native_eof)
        # Both archives must be complete before replacing any previous download.
        for filename in names:
            (stage / filename).replace(destination / filename)
    return names, sum(row.get('complete', '').lower() == 'true' for row in rows), len(rows)
