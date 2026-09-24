#!/usr/bin/env python
"""Expand solver configuration rows over the union of selected problem globs."""
from __future__ import annotations

import argparse
import csv
import glob
import os
from pathlib import Path
import sys
import tempfile

from slurmy import COLUMNS, SlurmyError, path_at, read_pairs


def expand(configurations: list[Path], patterns: list[str], output: Path) -> int:
    problems: set[Path] = set()
    for pattern in patterns:
        matches = [Path(os.path.abspath(p)) for p in glob.glob(os.path.expanduser(pattern), recursive=True) if Path(p).is_file()]
        if not matches:
            raise SlurmyError(f"problem glob matched no files: {pattern}")
        problems.update(matches)
    rows = []
    for filename in configurations:
        with filename.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            required = set(COLUMNS) - {"problem", "exclusive_cpu", "exclusive_node"}
            headers = reader.fieldnames or []
            if len(set(headers)) != len(headers) or not required <= set(headers) or set(headers) - (set(COLUMNS) - {"problem"}):
                raise SlurmyError(f"{filename}: configuration columns are {', '.join(c for c in COLUMNS if c != 'problem')}")
            for row in reader:
                if None in row or any(not row.get(key, '').strip() for key in required):
                    raise SlurmyError(f"{filename}: malformed configuration row")
                row['solver_directory'] = str(path_at(row['solver_directory'], filename.resolve().parent))
                for problem in sorted(problems):
                    rows.append({**row, "problem": str(problem)})
    if not rows:
        raise SlurmyError("no solver configurations")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", newline="", dir=output.parent,
                                         prefix=output.name + ".", suffix=".tmp", delete=False) as target:
            temporary = Path(target.name)
            writer = csv.DictWriter(target, fieldnames=COLUMNS)
            writer.writeheader()
            writer.writerows(rows)
        read_pairs(temporary.resolve())
        os.replace(temporary, output)
    except Exception:
        if temporary:
            temporary.unlink(missing_ok=True)
        raise
    return len(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configurations", required=True, action="append", type=Path)
    parser.add_argument("--problems", action="append", nargs="+", default=[])
    parser.add_argument("--problem-globs-file", type=Path, help="one glob per line, relative to this file")
    parser.add_argument("--output", type=Path, default=Path("jobpairs.csv"))
    args = parser.parse_args()
    try:
        patterns = [p for group in args.problems for p in group]
        if args.problem_globs_file:
            patterns.extend(str(path_at(line, args.problem_globs_file.resolve().parent))
                            for line in args.problem_globs_file.read_text().splitlines() if line.strip())
        if not patterns:
            raise SlurmyError("provide --problems or --problem-globs-file")
        count = expand(args.configurations, patterns, args.output)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Wrote {count} explicit jobpairs to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
