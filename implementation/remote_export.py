#!/usr/bin/env python3
"""Create the four-file, user-facing export inside a remote Slurmy job."""
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
import szs


RESULT_FIELDS = ('task_id', 'complete', 'status', 'return_code', 'wall_seconds',
                 'cpu_seconds', 'user_seconds', 'system_seconds', 'cpu_usage_percent',
                 'max_virtual_memory_kib', 'max_memory_kib', 'timed_out', 'memory_out',
                 'task_key', 'archive', 'szs_status', 'szs_output', 'postprocess_data',
                 'postprocess_return_code', 'postprocess_error', 'postprocess_status')
EXPORT_FIELDS = ('task_id', 'solver', 'problem', 'command', 'cores', 'threads_per_core',
                 'wc_limit', 'cpu_limit', 'mem_limit', 'complete', 'status',
                 'return_code', 'wall_seconds', 'cpu_seconds', 'user_seconds',
                 'system_seconds', 'cpu_usage_percent', 'max_virtual_memory_kib',
                 'max_memory_kib', 'timed_out', 'memory_out', 'szs_status',
                 'szs_output', 'has_solver_stdout', 'has_solver_stderr', 'szs_status_code', 'szs_output_code', 'execution_status',
                 'postprocess_return_code', 'postprocess_error')
JOB_ID = re.compile(r'^[A-Za-z0-9][A-Za-z0-9_.-]*$')
STAREXEC_FIELDS = (*EXPORT_FIELDS, 'has_combined_solver_output')
ARCHIVE = re.compile(r'^[A-Za-z0-9._-]+\.tar\.gz$')


def safe_name(value: str) -> str:
    return re.sub(r'[^A-Za-z0-9._-]+', '-', value).strip('._-')[:80] or 'job'


def export_names(metadata: dict) -> tuple[str, str, str, str]:
    name = safe_name(str(metadata.get('job_name') or metadata.get('job_id') or 'job'))
    return (f'{name}_results-summary.csv', f'{name}_solver-stdout.tgz',
            f'{name}_solver-stderr.tgz', f'{name}_postprocessing.tgz')


def stream_name(definition: dict, task_id: int, error: bool) -> str:
    solver = str(definition.get('solver') or 'solver').replace('/', '-')
    if solver in {'.', '..'} or '\x00' in solver:
        raise ValueError('Invalid solver configuration name for log export.')
    problem = Path(str(definition.get('problem') or 'problem')).name
    return f'{solver}___{problem}___{task_id:09d}{".stderr" if error else ""}.log'


def postprocess_data(row: dict) -> dict:
    """Only structured result metadata defines custom columns; logs stay opaque."""
    try:
        data = json.loads(row.get('postprocess_data') or '{}')
    except (ValueError, TypeError):
        return {}
    return data if isinstance(data, dict) else {}


def custom_columns(results: dict, fields: tuple) -> dict[str, str]:
    keys = sorted({key for row in results.values() for key in postprocess_data(row)} - {'status'})
    # Reserve original custom names before allocating prefixed collision names.
    reserved = set(fields) | set(RESULT_FIELDS) | set(STAREXEC_FIELDS)
    used = reserved | set(keys)
    columns = {}
    for key in keys:
        name = key
        if key in reserved:
            base = 'postprocess.' + key
            name, suffix = base, 2
            while name in used:
                name = f'{base}.{suffix}'
                suffix += 1
        used.add(name)
        columns[key] = name
    return columns


def custom_value(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True) if isinstance(value, (dict, list)) else value


def records(root: Path) -> dict[int, dict[str, str]]:
    found = {}
    for path in sorted((root / 'results').glob('batch_*_task_*.csv')):
        with path.open(newline='', encoding='utf-8') as stream:
            for row in csv.DictReader(stream):
                if row.get('task_id', '').isdigit():
                    found[int(row['task_id'])] = szs.normalize_row(row)
    return found


def export(root: Path, combined: bool = False) -> None:
    # StarExec staging must not replace the standard four-file download.
    directory = root / 'exports' / 'starexec' if combined else root / 'exports'
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    with (directory / '.lock').open('a+') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        source_files = [root / 'metadata.json', root / 'manifest.jsonl',
                        *sorted((root / 'results').glob('batch_*_task_*.csv')),
                        *sorted((root / 'results').glob('*.tar.gz'))]
        signature = hashlib.sha256(('streams-v3-postprocessing' + str(combined) + szs.REGISTRY_VERSION + repr([(path.name, path.stat().st_size, path.stat().st_mtime_ns)
                                         for path in source_files if path.is_file()])).encode()).hexdigest()
        signature_file = directory / '.signature'
        metadata = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
        names = export_names(metadata)
        if all((directory / name).is_file() for name in names) and signature_file.is_file():
            if signature_file.read_text(encoding='ascii').strip() == signature:
                print('Export unchanged; reusing the four existing files.', flush=True)
                print('EXPORT_FILES|' + '|'.join(names), flush=True)
                return
        definitions = {}
        with (root / 'manifest.jsonl').open(encoding='utf-8') as stream:
            for line in stream:
                item = json.loads(line)
                definitions[int(item['task_id'])] = item
        results = records(root)
        count = max(int(metadata.get('task_count', 0)), max(definitions, default=-1) + 1)
        with tempfile.TemporaryDirectory(prefix='.export-', dir=directory) as staging_name:
            staging = Path(staging_name)
            with tarfile.open(staging / names[1], 'w:gz') as stdout_tar, \
                 tarfile.open(staging / names[2], 'w:gz') as stderr_tar, \
                 tarfile.open(staging / names[3], 'w:gz') as postprocess_tar, \
                 (staging / names[0]).open('w', newline='', encoding='utf-8') as csv_file:
                for archive, filename in ((stdout_tar, names[1]), (stderr_tar, names[2]), (postprocess_tar, names[3])):
                    directory_info = tarfile.TarInfo(filename.removesuffix('.tgz') + '/')
                    directory_info.type = tarfile.DIRTYPE
                    directory_info.mode = 0o700
                    archive.addfile(directory_info)
                base_fields = STAREXEC_FIELDS if combined else EXPORT_FIELDS
                columns = custom_columns(results, base_fields)
                fields = (*base_fields, *columns.values())
                writer = csv.DictWriter(csv_file, fieldnames=fields)
                writer.writeheader()
                for task_id in range(count):
                    definition = definitions.get(task_id, {})
                    result = results.get(task_id, {})
                    row = {field: result.get(field, '') for field in RESULT_FIELDS}
                    data = postprocess_data(result)
                    row['execution_status'] = result.get('status', '')
                    row['status'] = custom_value(data.get('status', result.get('postprocess_status') or result.get('status', '')))
                    row.update({name: custom_value(data.get(key, '')) for key, name in columns.items()})
                    row.update(task_id=task_id, solver=definition.get('solver', ''),
                               problem=definition.get('problem', ''),
                               command=definition.get('command', ''),
                               cores=definition.get('cores', ''),
                               threads_per_core=definition.get('threads_per_core', 1),
                               wc_limit=definition.get('wc_limit', ''),
                               cpu_limit=definition.get('cpu_limit', ''),
                               mem_limit=definition.get('mem_limit', ''),
                               has_solver_stdout='false', has_solver_stderr='false',
                               has_combined_solver_output='false',
                               szs_status_code=szs.mnemonic(result.get('szs_status')),
                               szs_output_code=szs.data_mnemonic(result.get('szs_output')))
                    name = result.get('archive', '')
                    if ARCHIVE.fullmatch(name) and (root / 'results' / name).is_file():
                        with tarfile.open(root / 'results' / name, 'r:gz') as source:
                            members = source.getmembers() if combined else source
                            combined_member = (next((member for member in members if member.isfile()
                                                     and member.name.endswith('.solver-combined.log')), None)
                                               if combined else None)
                            for member in members:
                                if not member.isfile():
                                    continue
                                if combined_member is not None and member is combined_member:
                                    target, field = stdout_tar, 'has_solver_stdout'
                                    row['has_combined_solver_output'] = 'true'
                                elif member.name.endswith(('.postprocessor.log', '.postprocessor-stderr.log')):
                                    target, field = postprocess_tar, None
                                elif member.name.endswith('.solver.log'):
                                    if combined_member is not None:
                                        continue
                                    target, field = stdout_tar, 'has_solver_stdout'
                                elif member.name.endswith('.solver-stderr.log'):
                                    target, field = stderr_tar, 'has_solver_stderr'
                                else:
                                    continue
                                content = source.extractfile(member)
                                if content is None:
                                    continue
                                archive_root = names[3 if target is postprocess_tar else 2 if target is stderr_tar else 1].removesuffix('.tgz')
                                is_error = (member.name.endswith('.postprocessor-stderr.log')
                                            if target is postprocess_tar else target is stderr_tar)
                                info = tarfile.TarInfo(f'{archive_root}/{stream_name(definition, task_id, is_error)}')
                                info.size = member.size
                                info.mode = 0o600
                                target.addfile(info, content)
                                if field is not None:
                                    row[field] = 'true'
                    writer.writerow({field: row.get(field, '') for field in fields})
                    if (task_id + 1) % 1000 == 0 or task_id + 1 == count:
                        print(f'Exporting solver-problem pairs and logs: {task_id + 1}/{count}', flush=True)
            for name in names:
                os.replace(staging / name, directory / name)
            (staging / '.signature').write_text(signature + '\n', encoding='ascii')
            os.replace(staging / '.signature', signature_file)
        print(f'Export ready: {count} solver-problem pairs', flush=True)
        print('EXPORT_FILES|' + '|'.join(names), flush=True)


def main() -> int:
    if len(sys.argv) not in (2, 3) or not JOB_ID.fullmatch(sys.argv[1]) or (len(sys.argv) == 3 and sys.argv[2] != '--combined'):
        raise SystemExit('Expected a Slurmy job ID.')
    root = Path.home() / 'slurmy' / 'jobs' / sys.argv[1]
    if not root.is_dir() or root.is_symlink():
        raise SystemExit('Job directory not found or is a symlink.')
    export(root, combined=len(sys.argv) == 3)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
