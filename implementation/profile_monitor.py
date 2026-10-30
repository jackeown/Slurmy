#!/usr/bin/env python3
"""Profile one read-only job snapshot; emit timings, not solver or problem data."""
from __future__ import annotations

import argparse
import cProfile
import gzip
import json
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'web-app'))
from ssh_transport import connection_options
from slurmy_web.monitor import REMOTE_SNAPSHOT_SCRIPT, parse_snapshot

MARKERS = [
    ('set -u\n', 'begin'),
    ('        metadata_changed=1', 'metadata'),
    ('        indexed_results=', 'index listing'),
    ('        modern_results=0', 'summary / compatibility'),
    ('        if [[ "$job" == "$DETAIL" && ! -f', 'progress'),
    ('        if [[ -f "$directory/submission.csv" ]]; then', 'submission records'),
    ('            detail_csv_results=', 'result rows'),
    ('            if [[ -f "$directory/manifest.jsonl"', 'manifest'),
    ('            # Modern metadata', 'legacy mapping / logs'),
    ('    # Probe the kernel lock', 'controller check'),
    ('if queue_snapshot=', 'squeue'),
    ('accounting_options=()', 'sacct setup'),
    ("printf 'SACCT\\0'", 'sacct query'),
]


def instrument(script):
    for marker, name in MARKERS:
        if marker not in script:
            raise ValueError(f'Collector changed: profiling marker missing for {name}')
        stamp = f'printf "PROFILE|{name}|%s\\n" "${{EPOCHREALTIME:-$SECONDS}}" >&2\n'
        script = script.replace(marker, marker + stamp if name == 'begin' else stamp + marker, 1)
    return script + '\nprintf "PROFILE|done|%s\\n" "${EPOCHREALTIME:-$SECONDS}" >&2\n'


def remote_stages(stderr):
    stamps = []
    for line in stderr.splitlines():
        if line.startswith('PROFILE|'):
            _, name, stamp = line.split('|')
            stamps.append((name, float(stamp)))
    return [{'stage': name, 'seconds': max(0, following - stamp)}
            for (name, stamp), (_, following) in zip(stamps, stamps[1:])]


def profile(host, job, fresh_connection=False, save_payload=None, python_profile=None):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', host) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', job):
        raise ValueError('Invalid SSH alias or job ID.')
    remote = shlex.join(['bash', '-c', 'set -o pipefail; timeout -k 5 45 bash -s -- "$@" | gzip -1',
                         'slurmy-monitor-profile', job, ''])
    options = ['-S', 'none'] if fresh_connection else connection_options()
    started = time.perf_counter()
    result = subprocess.run(['ssh', *options, '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10',
                             '--', host, remote], input=instrument(REMOTE_SNAPSHOT_SCRIPT).encode(),
                            capture_output=True, timeout=55)
    ssh_seconds = time.perf_counter() - started
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors='replace').strip() or f'SSH exited {result.returncode}')
    started = time.perf_counter()
    payload = gzip.decompress(result.stdout)
    decompress_seconds = time.perf_counter() - started
    started = time.perf_counter()
    value = parse_snapshot(host, payload)
    parse_seconds = time.perf_counter() - started
    snapshot = value.find_job(job)
    if not snapshot:
        raise ValueError('Job not found in the collected snapshot.')
    if save_payload:
        Path(save_payload).write_bytes(result.stdout)
    if python_profile:
        profiler = cProfile.Profile()
        profiler.runcall(parse_snapshot, host, payload)
        profiler.dump_stats(python_profile)
    stages = remote_stages(result.stderr.decode(errors='replace'))
    return dict(ssh_seconds=ssh_seconds, remote_stages=stages,
                ssh_other_seconds=max(0, ssh_seconds - sum(stage['seconds'] for stage in stages)),
                decompress_seconds=decompress_seconds, parse_seconds=parse_seconds,
                compressed_bytes=len(result.stdout), uncompressed_bytes=len(payload),
                solver_problem_pairs=snapshot.task_count, result_rows=len(snapshot.results),
                fresh_connection=fresh_connection)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='datalab')
    parser.add_argument('--job', required=True)
    parser.add_argument('--fresh-connection', action='store_true', help='include a new SSH connection handshake')
    parser.add_argument('--output', type=Path, help='write numeric timing report as JSON')
    parser.add_argument('--save-payload', type=Path, help='optionally save job data locally for offline profiling')
    parser.add_argument('--python-profile', type=Path, help='optional local cProfile output; does not repeat SSH collection')
    args = parser.parse_args(argv)
    try:
        report = profile(args.host, args.job, args.fresh_connection, args.save_payload, args.python_profile)
    except (ValueError, RuntimeError, OSError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, f'{exc}\n')
    text = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
