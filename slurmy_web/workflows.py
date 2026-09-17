"""Validate browser input and write the same portable specifications as the CLI."""
from __future__ import annotations

import csv
import glob
import io
from pathlib import Path
import re
import shlex
import shutil
import tempfile

from slurmy import COLUMNS, generate, path_at, positive, read_builds, read_pairs

REPO = Path(__file__).resolve().parents[1]
BASE = REPO / 'YourRuns/example-generator'
GENERATED = REPO / 'YourRuns/GENERATED'


def alias(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', value):
        raise ValueError('Use an SSH alias/partition containing letters, numbers, dots, underscores or hyphens.')
    return value


def existing(value, kind='file', base=None):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('Enter an existing local path.')
    path = path_at(value.strip(), base or BASE)
    if not (path.is_dir() if kind == 'directory' else path.is_file()):
        raise ValueError(f'Not an existing {kind}: {path}')
    return path


def problem_glob(value):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('Enter a problem path or glob.')
    pattern = str(path_at(value.strip(), BASE))
    files = sorted({str(Path(p).resolve()) for p in glob.glob(pattern, recursive=True) if Path(p).is_file()})
    if not files:
        raise ValueError(f'No files matched {pattern}')
    return pattern, files


def csv_text(rows, columns=COLUMNS):
    stream = io.StringIO(newline='')
    writer = csv.DictWriter(stream, fieldnames=columns)
    writer.writeheader()
    writer.writerows(rows)
    return stream.getvalue()


def imported_rows(filename, configurations=False):
    with filename.open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        expected = set(COLUMNS) - ({'problem'} if configurations else set())
        if not reader.fieldnames or len(set(reader.fieldnames)) != len(reader.fieldnames) or set(reader.fieldnames) - expected:
            raise ValueError('Unexpected or duplicate CSV columns.')
        rows = list(reader)
    if not rows:
        raise ValueError('The CSV contains no rows.')
    for row in rows:
        if None in row:
            raise ValueError('Malformed CSV row.')
        row['solver_directory'] = str(existing(row.get('solver_directory'), 'directory', filename.parent))
        if not configurations:
            row['problem'] = str(existing(row.get('problem'), base=filename.parent))
    return rows


def specification(data):
    name = data.get('name', '')
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Workflow name: use lowercase letters, numbers and hyphens.')
    destination = GENERATED / ('example-' + name)
    if destination.exists():
        raise ValueError(f'Workflow already exists: {destination}')
    host, partition = alias(data.get('host')), alias(data.get('partition'))
    degree = positive(str(data.get('degree', '')), 'parallel calls per batch')
    mode = data.get('mode')
    configs, patterns = [], []
    if mode == 'jobpairs':
        filename = existing(data.get('jobpairs'))
        read_pairs(filename)
        rows = imported_rows(filename)
    elif mode in {'interactive', 'configurations'}:
        if mode == 'configurations':
            configs = imported_rows(existing(data.get('configurations_file')), True)
        else:
            configs = data.get('configurations', [])
            if not isinstance(configs, list) or not configs:
                raise ValueError('Add at least one solver configuration.')
            configs = [dict(row) for row in configs]
            for row in configs:
                row['solver_directory'] = str(existing(row.get('solver_directory'), 'directory'))
        problems = set()
        for value in data.get('globs', []):
            pattern, matches = problem_glob(value)
            patterns.append(pattern)
            problems.update(matches)
        if not problems:
            raise ValueError('Add at least one problem glob.')
        rows = [{**row, 'problem': problem} for row in configs for problem in sorted(problems)]
    else:
        raise ValueError('Choose how to define solver/problem calls.')
    recipes = {}
    if data.get('building_mode') == 'existing':
        builds = read_builds(existing(data.get('building_file')))
    elif data.get('building_mode') == 'create':
        builds = []
        for index, entry in enumerate(data.get('resources', [])):
            root = existing(entry.get('root'), 'directory')
            if any(root == previous for previous, _ in builds):
                raise ValueError(f'Duplicate resource root: {root}')
            choice = entry.get('mode')
            if choice == 'none':
                recipe = None
            elif choice == 'script':
                recipe = existing(entry.get('script'))
            elif choice == 'commands':
                commands = entry.get('commands', '').strip()
                if not commands:
                    raise ValueError('Enter the remote build commands.')
                recipe = destination / f'build-resource-{index}.sh'
                recipes[recipe.name] = '#!/usr/bin/env bash\nset -euo pipefail\n' + commands + '\n'
            else:
                raise ValueError('Choose whether and how to build each resource.')
            builds.append((root, recipe))
    else:
        raise ValueError('Choose how to define resource builds.')
    if data.get('limiter_mode') == 'existing':
        file = existing(data.get('limiter_file'))
        limiter, base = file.read_text().strip(), file.parent
    elif data.get('limiter_mode') == 'inline':
        limiter, base = data.get('limiter', '').strip(), BASE
    else:
        raise ValueError('Choose an existing limiter template or enter one.')
    lexer = shlex.shlex(limiter, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = ''
    executable_word = lexer.get_token()
    if not executable_word:
        raise ValueError('The limiter template is empty.')
    executable = path_at(executable_word, base)
    limiter = shlex.quote(str(executable)) + ' ' + limiter[lexer.instream.tell():]
    files = {'jobpairs.csv': csv_text(rows), 'resource_limiter_template.txt': limiter + '\n', **recipes}
    def building_text(staged=None):
        return ''.join(str(root) + '\n' + (str(staged / recipe.name) if staged and recipe and recipe.name in recipes and recipe.parent == destination
                      else str(recipe) if recipe else '') + '\n' for root, recipe in builds)
    files['building.txt'] = building_text()
    # Reuse the core validator/planner. Preview never builds, submits or keeps files.
    with tempfile.TemporaryDirectory(prefix='slurmy-preview-') as folder:
        staging = Path(folder)
        for filename, body in files.items():
            (staging / filename).write_text(body, encoding='utf-8')
        (staging / 'building.txt').write_text(building_text(staging))
        generate(staging / 'jobpairs.csv', staging / 'building.txt', staging / 'resource_limiter_template.txt', degree)
    files['Makefile'] = (f'REPO_ROOT := ../../..\nDEG_PAR := {degree}\nSLURMY_HOST := {host}\n'
                         f'SLURMY_PARTITION := {partition}\ninclude $(REPO_ROOT)/templates/workflow.mk\n')
    if configs:
        files['configurations.csv'] = csv_text(configs, tuple(c for c in COLUMNS if c != 'problem'))
        files['problem-globs.txt'] = '\n'.join(patterns) + '\n'
        files['Makefile'] += ('\n.PHONY: pairs\npairs:\n\tpython "$(REPO_ROOT)/slurmy-pairs.py" '
                             '--configurations configurations.csv --problem-globs-file problem-globs.txt --output jobpairs.csv\n')
    return destination, files, len(rows)


def save(data):
    destination, files, count = specification(data)
    destination.mkdir(parents=True)  # Atomic refusal if another request created it.
    try:
        for name, body in files.items():
            path = destination / name
            path.write_text(body, encoding='utf-8')
            if path.suffix == '.sh':
                path.chmod(0o755)
    except Exception:
        shutil.rmtree(destination)
        raise
    return destination, count
