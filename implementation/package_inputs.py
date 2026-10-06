#!/usr/bin/env python
"""Package local inputs with portable tar while reporting live byte counts."""
from __future__ import annotations

import argparse
import gzip
import os
from pathlib import Path
import subprocess
import time


def size(value: int) -> str:
    amount = float(value)
    unit = 'B'
    for unit in ('B', 'KiB', 'MiB', 'GiB', 'TiB'):
        if amount < 1024 or unit == 'TiB':
            break
        amount /= 1024
    return f'{amount:.1f} {unit}' if unit != 'B' else f'{value} B'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--paths', type=Path, help='NUL-separated paths relative to --base; omit to archive the entire base')
    args = parser.parse_args()
    command = ['tar', '-chf', '-', '-C', str(args.base)]
    command += ['--null', '-T', str(args.paths)] if args.paths else ['.']
    environment = {**os.environ, 'COPYFILE_DISABLE': '1'}
    started = time.monotonic()
    processed = 0
    last_report = started
    process = subprocess.Popen(command, stdout=subprocess.PIPE, env=environment)
    print(f'Packaging progress: {args.label}: starting', flush=True)
    try:
        with args.output.open('wb') as output, gzip.GzipFile(fileobj=output, mode='wb') as compressor:
            assert process.stdout is not None
            while chunk := process.stdout.read(1024 * 1024):
                compressor.write(chunk)
                processed += len(chunk)
                now = time.monotonic()
                if now - last_report >= 1:
                    output.flush()
                    print(f'Packaging progress: {args.label}: {size(processed)} processed, '
                          f'{size(output.tell())} compressed, {int(now - started)}s elapsed', flush=True)
                    last_report = now
        code = process.wait()
        if code:
            raise RuntimeError(f'tar failed with exit code {code} while packaging {args.label}')
        print(f'Packaging progress: {args.label}: complete, {size(processed)} processed, '
              f'{size(args.output.stat().st_size)} compressed, {int(time.monotonic() - started)}s elapsed', flush=True)
        return 0
    except Exception:
        args.output.unlink(missing_ok=True)
        raise
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait()


if __name__ == '__main__':
    raise SystemExit(main())
