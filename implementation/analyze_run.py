"""Run a workflow analyzer and normalize its output for the pair result."""
from __future__ import annotations

import csv
import filecmp
import io
import json
from pathlib import Path
import subprocess
import sys

SCRIPT_NAMES = ('analysis.py', 'analysis.sh')
TIMEOUT_SECONDS = 60
KILL_GRACE_SECONDS = 5


def workflow_script(directory: Path) -> Path | None:
    scripts = [directory / name for name in SCRIPT_NAMES if (directory / name).exists()]
    if len(scripts) > 1:
        raise ValueError('Choose one workflow analysis script (analysis.py or analysis.sh).')
    if scripts and not scripts[0].is_file():
        raise ValueError(f'Analysis script is not a file: {scripts[0]}')
    return scripts[0] if scripts else None


def prepared_scripts_current(assets: Path) -> bool:
    manifest = assets / 'analysis-sources.json'
    if not manifest.is_file():
        return not (assets / 'analysis-settings.sh').is_file()
    try:
        sources = json.loads(manifest.read_text(encoding='utf-8'))
        return all(filecmp.cmp(source, assets / name, shallow=False) for source, name in sources.items())
    except (OSError, ValueError):
        return False


def named_values(names, values):
    if len(set(names)) != len(names) or any(not name.strip() for name in names):
        raise ValueError('Analysis column names must be nonempty and unique.')
    return dict(zip(names, values))


def json_object(pairs):
    return named_values([name for name, _ in pairs], [value for _, value in pairs])


def parse_columns(value: str) -> list[str]:
    if not value or not value.strip():
        return []
    try:
        rows = list(csv.reader(io.StringIO(value, newline=''), skipinitialspace=True, strict=True))
        if len(rows) != 1:
            raise ValueError('Enter the analysis columns on one line.')
        names = [name.strip() for name in rows[0]]
        named_values(names, names)
        return names
    except csv.Error as exc:
        raise ValueError(f'Invalid analysis column list: {exc}') from exc


def parse_result(output: str) -> dict:
    if not output.strip():
        return {}
    if output.lstrip().startswith('{'):
        values = json.loads(output, object_pairs_hook=json_object)
        if any(isinstance(value, (list, dict)) for value in values.values()):
            raise ValueError('Analysis JSON values must be scalars.')
        return values
    rows = csv.reader(io.StringIO(output, newline=''), strict=True)
    names = next(rows)
    values = next(rows, [])
    if len(names) != len(values) or not names or next(rows, None) is not None:
        raise ValueError('Analysis CSV needs a header and exactly one data row.')
    return named_values(names, values)


def main() -> int:
    if sys.argv[1] == '--check-prepared':
        return 0 if prepared_scripts_current(Path(sys.argv[2])) else 1
    script = Path(sys.argv[1])
    interpreter = sys.executable if script.suffix == '.py' else 'bash'
    columns = json.loads(sys.argv[2])
    logs = [path if Path(path).is_file() else '/dev/null' for path in sys.argv[3:]]
    try:
        result = subprocess.run([interpreter, str(script), *logs], stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE)
        if result.returncode:
            raise ValueError(f'Script exited with status {result.returncode}.')
        values = parse_result(result.stdout.decode('utf-8-sig'))
        unexpected = values.keys() - columns if columns else set()
        if unexpected:
            raise ValueError('Unexpected analysis columns: ' + ', '.join(sorted(unexpected)))
        output = json.dumps(values, allow_nan=False, separators=(',', ':'))
        if len(output) > csv.field_size_limit():
            raise ValueError('Analysis output is too large for a result CSV field.')
        print(output)
    except (OSError, ValueError, csv.Error) as exc:
        print(f'Analysis: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
