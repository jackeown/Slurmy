#!/usr/bin/env python3
"""Create the three-file, user-facing export inside a remote Slurmy job."""
from __future__ import annotations

import csv
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tarfile
import tempfile


RESULT_FIELDS = ('task_id', 'complete', 'status', 'return_code', 'wall_seconds',
                 'cpu_seconds', 'user_seconds', 'system_seconds', 'cpu_usage_percent',
                 'max_virtual_memory_kib', 'max_memory_kib', 'timed_out', 'memory_out',
                 'task_key', 'archive', 'szs_status', 'szs_output')
EXPORT_FIELDS = ('task_id', 'solver', 'problem', 'command', 'cores', 'threads_per_core',
                 'wc_limit', 'cpu_limit', 'mem_limit', 'complete', 'status',
                 'return_code', 'wall_seconds', 'cpu_seconds', 'user_seconds',
                 'system_seconds', 'cpu_usage_percent', 'max_virtual_memory_kib',
                 'max_memory_kib', 'timed_out', 'memory_out', 'szs_status',
                 'szs_output', 'has_solver_stdout', 'has_solver_stderr')
JOB_ID = re.compile(r'^[A-Za-z0-9][A-Za-z0-9_.-]*$')
ARCHIVE = re.compile(r'^[A-Za-z0-9._-]+\.tar\.gz$')


def records(root: Path) -> dict[int, dict[str, str]]:
    found = {}
    for path in sorted((root / 'results').glob('batch_*_task_*.csv')):
        with path.open(newline='', encoding='utf-8') as stream:
            for row in csv.DictReader(stream):
                if row.get('task_id', '').isdigit():
                    found[int(row['task_id'])] = row
    return found


def export(root: Path) -> None:
    directory = root / 'exports'
    directory.mkdir(mode=0o700, exist_ok=True)
    with (directory / '.lock').open('a+') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        source_files = [root / 'metadata.json', root / 'manifest.jsonl',
                        *sorted((root / 'results').glob('batch_*_task_*.csv')),
                        *sorted((root / 'results').glob('*.tar.gz'))]
        signature = hashlib.sha256(repr([(path.name, path.stat().st_size, path.stat().st_mtime_ns)
                                         for path in source_files if path.is_file()]).encode()).hexdigest()
        signature_file = directory / '.signature'
        names = ('calls.csv', 'solver-logs.tgz', 'solver-errors.tgz')
        if all((directory / name).is_file() for name in names) and signature_file.is_file():
            if signature_file.read_text(encoding='ascii').strip() == signature:
                print('Export unchanged; reusing the three existing files.', flush=True)
                return
        metadata = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
        definitions = {}
        with (root / 'manifest.jsonl').open(encoding='utf-8') as stream:
            for line in stream:
                item = json.loads(line)
                definitions[int(item['task_id'])] = item
        results = records(root)
        count = max(int(metadata.get('task_count', 0)), max(definitions, default=-1) + 1)
        with tempfile.TemporaryDirectory(prefix='.export-', dir=directory) as staging_name:
            staging = Path(staging_name)
            with tarfile.open(staging / 'solver-logs.tgz', 'w:gz') as stdout_tar, \
                 tarfile.open(staging / 'solver-errors.tgz', 'w:gz') as stderr_tar, \
                 (staging / 'calls.csv').open('w', newline='', encoding='utf-8') as csv_file:
                writer = csv.DictWriter(csv_file, fieldnames=EXPORT_FIELDS)
                writer.writeheader()
                for task_id in range(count):
                    definition = definitions.get(task_id, {})
                    result = results.get(task_id, {})
                    row = {field: result.get(field, '') for field in RESULT_FIELDS}
                    row.update(task_id=task_id, solver=definition.get('solver', ''),
                               problem=definition.get('problem', ''),
                               command=definition.get('command', ''),
                               cores=definition.get('cores', ''),
                               threads_per_core=definition.get('threads_per_core', 1),
                               wc_limit=definition.get('wc_limit', ''),
                               cpu_limit=definition.get('cpu_limit', ''),
                               mem_limit=definition.get('mem_limit', ''),
                               has_solver_stdout='false', has_solver_stderr='false')
                    name = result.get('archive', '')
                    if ARCHIVE.fullmatch(name) and (root / 'results' / name).is_file():
                        with tarfile.open(root / 'results' / name, 'r:gz') as source:
                            for member in source:
                                if not member.isfile():
                                    continue
                                if member.name.endswith('.solver.log'):
                                    target, field, suffix = stdout_tar, 'has_solver_stdout', '.log'
                                elif member.name.endswith('.solver-stderr.log'):
                                    target, field, suffix = stderr_tar, 'has_solver_stderr', '.stderr.log'
                                else:
                                    continue
                                content = source.extractfile(member)
                                if content is None:
                                    continue
                                info = tarfile.TarInfo(f'task_{task_id:09d}{suffix}')
                                info.size = member.size
                                info.mode = 0o600
                                target.addfile(info, content)
                                row[field] = 'true'
                    writer.writerow({field: row.get(field, '') for field in EXPORT_FIELDS})
                    if (task_id + 1) % 1000 == 0 or task_id + 1 == count:
                        print(f'Exporting calls and logs: {task_id + 1}/{count}', flush=True)
            for name in names:
                os.replace(staging / name, directory / name)
            (staging / '.signature').write_text(signature + '\n', encoding='ascii')
            os.replace(staging / '.signature', signature_file)
        print(f'Export ready: {count} calls', flush=True)


def main() -> int:
    if len(sys.argv) != 2 or not JOB_ID.fullmatch(sys.argv[1]):
        raise SystemExit('Expected a Slurmy job ID.')
    root = Path.home() / 'slurmy' / 'jobs' / sys.argv[1]
    if not root.is_dir() or root.is_symlink():
        raise SystemExit('Job directory not found or is a symlink.')
    export(root)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
