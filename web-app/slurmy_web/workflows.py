"""Validate browser input and write the same portable specifications as the CLI."""
from __future__ import annotations

import csv
import io
import json
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import tempfile
import time

from slurmy import COLUMNS, PAIR_ORDERS, generate, order_jobpairs, path_at, positive, read_builds, read_pairs
from slurmy_archives import expand_axiom_pattern, expand_problem_pattern

REPO = Path(__file__).resolve().parents[2]
USER_RUNS = REPO / 'workflows/my-workflows'
BASE = USER_RUNS
LEGACY_RUNS = USER_RUNS / 'GENERATED'
DELETED = REPO / 'workflows/my-workflows/.deleted-workflows'


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


def expand_repo_root(value):
    """Resolve Make-style repository references saved by older workflows."""
    text = str(value).strip()
    match = re.search(r'\$\((?:REPO_ROOT)\)|\$\{(?:REPO_ROOT)\}', text)
    if match:
        suffix = text[match.end():].lstrip('/\\')
        return REPO / suffix
    return None


def problem_glob(value, materialize=True, progress=None):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('Enter a problem path or glob.')
    pattern = str(expand_repo_root(value.strip()) or path_at(value.strip(), BASE))
    files = [str(path) for path in expand_problem_pattern(pattern, materialize, progress)]
    return pattern, files


def csv_text(rows, columns=COLUMNS):
    stream = io.StringIO(newline='')
    writer = csv.DictWriter(stream, fieldnames=columns)
    writer.writeheader()
    writer.writerows([{**row, 'solver_name': row.get('solver_name', '')} for row in rows])
    return stream.getvalue()


def imported_rows(filename, configurations=False, check_paths=True):
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
        row['solver_directory'] = str(existing(row.get('solver_directory'), 'directory', filename.parent)
                                      if check_paths else path_at(row.get('solver_directory', ''), filename.parent))
        if not configurations:
            row['problem'] = str(existing(row.get('problem'), base=filename.parent)
                                 if check_paths else path_at(row.get('problem', ''), filename.parent))
    return rows


def specification(data, destination_override=None, progress=None):
    data = dict(data)
    data['resources'] = [{**entry, 'role': 'solver' if entry.get('role') == 'prover' else entry.get('role')}
                         for entry in data.get('resources', [])]
    name = data.get('name', '')
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Workflow name: use lowercase letters, numbers and hyphens.')
    destination = destination_override or USER_RUNS / name
    if destination_override is None and destination.exists():
        raise ValueError(f'Workflow already exists: {destination}')
    host, partition = alias(data.get('host')), alias(data.get('partition'))
    batch_size = positive(str(data.get('batch_size') or 1), 'jobpairs per Slurm task')
    pair_order = data.get('pair_order') or 'solver-major'
    if pair_order not in PAIR_ORDERS:
        raise ValueError('Choose Problem Major, Solver Major, or random jobpair order.')
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
                row['cpus'] = 'auto'
                solver_name = str(row.get('solver_name') or '').strip()
                if not solver_name:
                    raise ValueError('Give each solver configuration a name.')
                if any(ord(char) < 32 or ord(char) == 127 for char in solver_name) or len(solver_name) > 80:
                    raise ValueError('Solver configuration names must be at most 80 characters and contain no control characters.')
                row['solver_name'] = solver_name
            names = [row['solver_name'].casefold() for row in configs]
            if len(names) != len(set(names)):
                raise ValueError('Give each solver configuration a distinct name.')
        problems = set()
        if progress:
            progress('phase', 'Finding selected problems and inspecting archives', 0, 0)
        for value in data.get('globs', []):
            pattern, matches = problem_glob(value, progress=progress)
            patterns.append(pattern)
            problems.update(matches)
        if not problems:
            raise ValueError('Add at least one problem glob.')
        rows = [{**row, 'problem': problem} for row in configs for problem in sorted(problems)]
    else:
        raise ValueError('Choose how to define solver/problem calls.')
    rows = order_jobpairs(rows, pair_order)
    axiom_patterns = []
    if data.get('axiom_mode') == 'custom' and not data.get('axiom_globs'):
        raise ValueError('Add at least one custom axiom glob, or use the cluster TPTP axioms.')
    for value in data.get('axiom_globs', []):
        if not isinstance(value, str) or not value.strip():
            raise ValueError('Enter an axiom path or glob.')
        pattern = str(expand_repo_root(value.strip()) or path_at(value.strip(), BASE))
        expand_axiom_pattern(pattern, materialize=False)
        axiom_patterns.append(pattern)
    recipes = {}
    if data.get('building_mode') == 'existing':
        builds = read_builds(existing(data.get('building_file')))
    elif data.get('building_mode') == 'create':
        builds = []
        for index, entry in enumerate(data.get('resources', [])):
            root = existing(entry.get('root'), 'directory')
            if any(root == previous.root for previous in builds):
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
            source = existing(entry.get('source'), 'directory') if entry.get('source', '').strip() and recipe else None
            if mode == 'interactive' and choice == 'script' and entry.get('role') == 'solver' and source is None:
                raise ValueError('A solver build needs a source directory.')
            artifact = entry.get('artifact', '').strip() if recipe else ''
            if mode == 'interactive' and choice == 'script' and entry.get('role') == 'solver':
                matching = [row for row in configs if path_at(row['solver_directory'], BASE) == root]
                if not matching:
                    raise ValueError(f'No solver configuration uses build root {root}')
                candidates = set()
                for row in matching:
                    words = shlex.split(row['command'])
                    if not words:
                        raise ValueError('Solver command is empty.')
                    executable = path_at(words[0], root)
                    if not executable.is_relative_to(root) or executable == root:
                        raise ValueError('A built solver command must start with an executable inside its solver root.')
                    candidates.add(str(executable.relative_to(root)))
                if len(candidates) != 1:
                    raise ValueError(f'Solver configurations using {root} must use the same executable.')
                artifact = candidates.pop()
            artifact_path = PurePosixPath(artifact) if artifact else None
            if artifact_path and (artifact_path.is_absolute() or not artifact_path.parts or '..' in artifact_path.parts or '\n' in artifact):
                raise ValueError('Expected executable must be a relative path inside the solver root.')
            from slurmy import BuildSpec
            builds.append(BuildSpec(root, recipe, source or (root if recipe and not artifact else None), artifact or None))
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
    if axiom_patterns:
        files['axiom-globs.txt'] = '\n'.join(axiom_patterns) + '\n'
    def building_text(staged=None):
        if data.get('building_mode') == 'existing' and not any(build.artifact for build in builds):
            return ''.join(str(build.root) + '\n' + (str(build.recipe) if build.recipe else '') + '\n' for build in builds)
        return json.dumps([{'root': str(build.root), 'source': str(build.source) if build.source else '',
                            'recipe': str(staged / build.recipe.name) if staged and build.recipe and build.recipe.name in recipes and build.recipe.parent == destination else str(build.recipe) if build.recipe else '',
                            'artifact': build.artifact or '', 'legacy': bool(build.recipe and not build.artifact)} for build in builds], indent=2) + '\n'
    files['building.txt'] = building_text()
    # Reuse the core validator/planner. Preview never builds, submits or keeps files.
    with tempfile.TemporaryDirectory(prefix='slurmy-preview-') as folder:
        staging = Path(folder)
        for filename, body in files.items():
            (staging / filename).write_text(body, encoding='utf-8')
        (staging / 'building.txt').write_text(building_text(staging))
        if progress:
            progress('phase', 'Checking calls and generating submission scripts', 0, 0)
        generate(staging / 'jobpairs.csv', staging / 'building.txt', staging / 'resource_limiter_template.txt',
                 staging / 'axiom-globs.txt' if axiom_patterns else None, batch_size, progress=progress)
    files['Makefile'] = (f'REPO_ROOT := ../../..\nBATCH_SIZE := {batch_size}\n'
                         + (f'PAIR_ORDER := {pair_order}\n' if configs else '') +
                         f'SLURMY_HOST := {host}\n'
                         f'SLURMY_PARTITION := {partition}\ninclude $(REPO_ROOT)/implementation/templates/workflow.mk\n')
    if axiom_patterns:
        files['Makefile'] = 'AXIOMS := axiom-globs.txt\n' + files['Makefile']
    if configs:
        files['configurations.csv'] = csv_text(configs, tuple(c for c in COLUMNS if c != 'problem'))
        files['problem-globs.txt'] = '\n'.join(patterns) + '\n'
        files['Makefile'] += ('\n.PHONY: pairs refresh-problems\npairs: jobpairs.csv\n\n'
                             'jobpairs.csv: configurations.csv problem-globs.txt refresh-problems\n'
                             '\tpython "$(REPO_ROOT)/implementation/slurmy-pairs.py" --configurations configurations.csv '
                             '--problem-globs-file problem-globs.txt --order "$(PAIR_ORDER)" --output $@\n')
    files['.slurmy-workflow.json'] = json.dumps(data, indent=2, sort_keys=True) + '\n'
    return destination, files, len(rows)


def save(data, destination=None, progress=None):
    data = dict(data)
    if '_created_at' not in data:
        data['_created_at'] = time.time()
    destination, files, count = specification(data, destination, progress)
    if progress:
        progress('phase', 'Saving workflow files', 0, 0)
    created = not destination.exists()
    if created:
        destination.mkdir(parents=True)  # Atomic refusal if another request created it.
    try:
        for name, body in files.items():
            path = destination / name
            temporary = path.with_name('.' + path.name + '.slurmy-new')
            temporary.write_text(body, encoding='utf-8')
            temporary.replace(path)
            if path.suffix == '.sh':
                path.chmod(0o755)
    except Exception:
        if created:
            shutil.rmtree(destination)
        raise
    return destination, count


def rename_and_save(data, source, progress=None):
    """Validate, rename, and update a workflow while retaining its former paths."""
    name = data.get('name', '')
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Workflow name: use lowercase letters, numbers and hyphens.')
    destination = USER_RUNS / name
    if destination == source:
        manifest = source / '.slurmy-workflow.json'
        if manifest.is_file():
            previous = json.loads(manifest.read_text(encoding='utf-8')).get('_previous_directories', [])
            if previous:
                data = {**data, '_previous_directories': previous}
        return save(data, source, progress)
    if destination.exists():
        raise ValueError(f'Workflow already exists: {destination}')

    _, files, count = specification(data, source, progress)
    if progress:
        progress('phase', 'Saving workflow files', 0, 0)
    previous = []
    old_manifest = source / '.slurmy-workflow.json'
    if old_manifest.is_file():
        previous = json.loads(old_manifest.read_text(encoding='utf-8')).get('_previous_directories', [])
    data = dict(data)

    def remap(value):
        if isinstance(value, str):
            return value.replace(str(source), str(destination))
        if isinstance(value, list):
            return [remap(item) for item in value]
        if isinstance(value, dict):
            return {key: remap(item) for key, item in value.items()}
        return value
    data = remap(data)
    data['_previous_directories'] = [*previous, str(source)]
    files = {filename: body.replace(str(source), str(destination)) for filename, body in files.items()}
    files['.slurmy-workflow.json'] = json.dumps(data, indent=2, sort_keys=True) + '\n'

    source.rename(destination)
    try:
        for filename, body in files.items():
            target = destination / filename
            temporary = target.with_name('.' + target.name + '.slurmy-new')
            temporary.write_text(body, encoding='utf-8')
            temporary.replace(target)
            if target.suffix == '.sh':
                target.chmod(0o755)
    except Exception:
        destination.rename(source)
        raise
    return destination, count


def delete_workflow(source):
    """Remove a user workflow from the UI by moving it to a recoverable archive."""
    if source.parent != USER_RUNS.resolve():
        raise ValueError('Only user workflows can be deleted.')
    DELETED.mkdir(parents=True, exist_ok=True)
    archived = DELETED / f'{source.name}-{time.time_ns()}'
    source.rename(archived)
    return archived


def migrate_user_runs():
    """Move older workflows up one directory, retaining their job-history links."""
    if not LEGACY_RUNS.is_dir():
        return
    sources = [path for path in sorted(LEGACY_RUNS.iterdir())
               if path.is_dir() and (path / 'Makefile').is_file()]
    for source in sources:
        if (USER_RUNS / source.name).exists():
            raise ValueError(f'Cannot migrate {source}: {USER_RUNS / source.name} already exists.')
    mappings = {str(source): str(USER_RUNS / source.name) for source in sources}
    for migrated in USER_RUNS.iterdir():
        manifest = migrated / '.slurmy-workflow.json'
        if migrated.is_dir() and manifest.is_file():
            for old in json.loads(manifest.read_text()).get('_previous_directories', []):
                if Path(old).parent == LEGACY_RUNS:
                    mappings[old] = str(migrated)
    def remap(value):
        if isinstance(value, str):
            for old, new in sorted(mappings.items(), key=lambda item: len(item[0]), reverse=True):
                value = value.replace(old, new)
            return value
        if isinstance(value, list):
            return [remap(item) for item in value]
        if isinstance(value, dict):
            return {key: remap(item) for key, item in value.items()}
        return value
    old_base = USER_RUNS / 'example-generator'
    plans = []
    for source in sources:
        manifest = source / '.slurmy-workflow.json'
        data = json.loads(manifest.read_text()) if manifest.is_file() else decisions(source, check_paths=False)
        data['name'] = source.name
        data.setdefault('_created_at', source.stat().st_ctime)
        # Saved browser decisions may retain paths relative to the former
        # generator directory. Normalize them before changing the form's base.
        for key in ('jobpairs', 'configurations_file', 'building_file', 'limiter_file'):
            if data.get(key):
                data[key] = str(path_at(data[key], old_base))
        for config in data.get('configurations', []):
            config['solver_directory'] = str(path_at(config['solver_directory'], old_base))
        for resource in data.get('resources', []):
            for key in ('root', 'script'):
                if resource.get(key):
                    resource[key] = str(path_at(resource[key], old_base))
        data['globs'] = [str(expand_repo_root(value) or path_at(value, old_base))
                         for value in data.get('globs', [])]
        data['axiom_globs'] = [str(expand_repo_root(value) or path_at(value, old_base))
                                for value in data.get('axiom_globs', [])]
        if data.get('limiter_mode') == 'inline':
            invocation = data.get('limiter', '')
            lexer = shlex.shlex(invocation, posix=True)
            lexer.whitespace_split = True
            lexer.commenters = ''
            executable = lexer.get_token()
            if executable:
                data['limiter'] = shlex.quote(str(path_at(executable, old_base))) + ' ' + invocation[lexer.instream.tell():]
        destination = USER_RUNS / source.name
        previous = data.get('_previous_directories', [])
        data = remap(data)
        data['_previous_directories'] = [*previous, str(source)]
        files = {}
        # Relocate definitions without validating resources: a workflow with a
        # Missing solver/problem inputs must still migrate and remain editable.
        for filename in ('jobpairs.csv', 'configurations.csv'):
            file = source / filename
            if not file.is_file():
                continue
            with file.open(encoding='utf-8-sig', newline='') as stream:
                reader = csv.DictReader(stream)
                columns, rows = reader.fieldnames, list(reader)
            for row in rows:
                for key in ('solver_directory', 'problem'):
                    if row.get(key):
                        row[key] = str(path_at(row[key], source))
            files[filename] = remap(csv_text(rows, columns))
        file = source / 'building.txt'
        files[file.name] = remap(''.join(str(path_at(line.strip(), source)) + '\n' if line.strip() else '\n'
                                        for line in file.read_text().splitlines()))
        file = source / 'resource_limiter_template.txt'
        invocation = file.read_text().strip()
        lexer = shlex.shlex(invocation, posix=True)
        lexer.whitespace_split = True
        lexer.commenters = ''
        executable = lexer.get_token()
        if executable:
            invocation = shlex.quote(str(path_at(executable, source))) + ' ' + invocation[lexer.instream.tell():]
        files[file.name] = remap(invocation) + '\n'
        file = source / 'problem-globs.txt'
        if file.is_file():
            files[file.name] = remap(''.join(str(expand_repo_root(line) or path_at(line, source)) + '\n'
                                            for line in file.read_text().splitlines() if line.strip()))
        file = source / 'axiom-globs.txt'
        if file.is_file():
            files[file.name] = remap(''.join(str(expand_repo_root(line) or path_at(line, source)) + '\n'
                                            for line in file.read_text().splitlines() if line.strip()))
        files['Makefile'] = re.sub(r'(?m)^REPO_ROOT\s*:?=.*$', 'REPO_ROOT := ../../..',
                                   remap((source / 'Makefile').read_text()), count=1)
        for recipe in source.glob('build-resource-*.sh'):
            files[recipe.name] = remap(recipe.read_text())
        files['.slurmy-workflow.json'] = json.dumps(data, indent=2, sort_keys=True) + '\n'
        plans.append((source, destination, files))
    for source, destination, files in plans:
        source.rename(destination)
        for filename, body in files.items():
            target = destination / filename
            temporary = target.with_name('.' + target.name + '.slurmy-new')
            temporary.write_text(body, encoding='utf-8')
            temporary.replace(target)
            if target.suffix == '.sh':
                target.chmod(0o755)
    # Unknown files are left intact rather than silently discarded.
    if not any(LEGACY_RUNS.iterdir()):
        LEGACY_RUNS.rmdir()


def resource_roles(data):
    """Place legacy build declarations next to the command they support."""
    data = dict(data)
    limiter_path = None
    try:
        if data.get('limiter_mode') == 'existing':
            template = path_at(data.get('limiter_file', ''), BASE)
            invocation, base = template.read_text(), template.parent
        else:
            invocation, base = data.get('limiter', ''), BASE
        words = shlex.split(invocation)
        if words:
            limiter_path = path_at(words[0], base)
    except (OSError, ValueError):
        # An unavailable template must remain editable so its path can be fixed.
        pass
    resources = []
    for entry in data.get('resources', []):
        entry = dict(entry)
        root = path_at(entry.get('root', ''), BASE)
        if entry.get('role') == 'prover':
            entry['role'] = 'solver'
        if entry.get('role') not in {'solver', 'limiter'}:
            entry['role'] = 'limiter' if limiter_path and limiter_path.is_relative_to(root) else 'solver'
        if entry['role'] == 'solver' and entry.get('mode') == 'script' and not entry.get('source'):
            entry['source'] = str(root)
        if entry.get('mode') in {'script', 'commands'} and not entry.get('artifact'):
            executable = limiter_path if entry['role'] == 'limiter' else None
            if executable is None:
                for config in data.get('configurations', []):
                    if path_at(config.get('solver_directory', ''), BASE) != root:
                        continue
                    words = shlex.split(config.get('command', ''))
                    if words:
                        executable = path_at(words[0], root)
                        break
            if executable and executable.is_relative_to(root):
                entry['artifact'] = str(executable.relative_to(root))
        resources.append(entry)
    data['resources'] = resources
    return data


def decisions(directory, check_paths=True):
    """Load saved builder decisions, reconstructing older workflows when necessary."""
    manifest = directory / '.slurmy-workflow.json'
    if manifest.is_file():
        data = json.loads(manifest.read_text(encoding='utf-8'))
        data['name'] = directory.name
        data = resource_roles(data)
        if any(item.get('role') == 'solver' and item.get('mode') == 'commands' for item in data.get('resources', [])):
            builds = {build.root: build for build in read_builds(directory / 'building.txt', check_paths=False)}
            for item in data['resources']:
                if item.get('role') == 'solver' and item.get('mode') == 'commands':
                    build = builds.get(path_at(item['root'], BASE))
                    if build and build.recipe:
                        item.update(mode='script', script=str(build.recipe), source=item.get('source') or str(build.root))
        return data
    makefile = (directory / 'Makefile').read_text(encoding='utf-8')
    def setting(name, default):
        match = re.search(rf'(?m)^{name}\s*(?:\?|:)?=\s*(\S+)', makefile)
        return match.group(1) if match else default
    config_file, globs_file = directory / 'configurations.csv', directory / 'problem-globs.txt'
    axiom_file = directory / 'axiom-globs.txt'
    axiom_globs = [str(expand_repo_root(line) or path_at(line, directory))
                   for line in axiom_file.read_text().splitlines() if line.strip()] if axiom_file.is_file() else []
    glob_values = [line.strip() for line in globs_file.read_text().splitlines() if line.strip()] if globs_file.is_file() else []
    if not glob_values:
        match = re.search(r'--problems\s+(.+?)\s+--(?:order|output)(?:\s|$)', makefile)
        if match:
            glob_values = shlex.split(match.group(1))
    if config_file.is_file() and glob_values:
        configs = imported_rows(config_file, True, check_paths)
        mode = 'interactive'
        globs = [str(expand_repo_root(value) or path_at(value, directory)) for value in glob_values]
        jobpairs = ''
    else:
        configs, globs, mode, jobpairs = [], [], 'jobpairs', str(directory / 'jobpairs.csv')
    builds = read_builds(directory / 'building.txt', check_paths=check_paths)
    resources = [dict(root=str(build.root), mode='script' if build.recipe else 'none',
                      script=str(build.recipe) if build.recipe else '', commands='',
                      source=str(build.source) if build.source else '', artifact=build.artifact or '') for build in builds]
    limiter = (directory / 'resource_limiter_template.txt').read_text(encoding='utf-8').strip()
    lexer = shlex.shlex(limiter, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = ''
    executable_word = lexer.get_token()
    if executable_word:
        limiter = shlex.quote(str(path_at(executable_word, directory))) + ' ' + limiter[lexer.instream.tell():]
    return resource_roles(dict(name=directory.name, host=setting('SLURMY_HOST', 'datalab'),
                partition=setting('SLURMY_PARTITION', 'CPU-amd'), batch_size=setting('BATCH_SIZE', '1'),
                pair_order=setting('PAIR_ORDER', 'solver-major'),
                mode=mode, jobpairs=jobpairs, configurations_file='', configurations=configs,
                globs=globs, axiom_globs=axiom_globs, axiom_mode='custom' if axiom_globs else 'cluster', building_mode='create', building_file='', resources=resources,
                limiter_mode='inline', limiter_file='', limiter=limiter))


def copyable_solver_configurations(directory):
    """Return editable solver cards from a saved workflow, including build settings."""
    data = decisions(directory, check_paths=False)
    configs = data.get('configurations') or []
    if not configs and (directory / 'jobpairs.csv').is_file():
        configs = imported_rows(directory / 'jobpairs.csv', check_paths=False)
    resources = {str(path_at(item.get('root', ''), BASE)): item
                 for item in data.get('resources', []) if item.get('role') == 'solver'}
    fields = ('solver_name', 'solver_directory', 'command', 'wc_limit', 'cpu_limit', 'mem_limit',
              'cores', 'exclusive_cpu', 'exclusive_node')
    seen = set()
    result = []
    for config in configs:
        card = {name: str(config.get(name) or ('false' if name in {'exclusive_cpu', 'exclusive_node'} else ''))
                for name in fields}
        if not card['solver_name']:
            card['solver_name'] = Path(shlex.split(card['command'])[0]).name if card['command'].strip() else 'Solver'
        card['solver_directory'] = str(path_at(card['solver_directory'], directory))
        key = tuple(card.values())
        if key in seen:
            continue
        seen.add(key)
        build = resources.get(card['solver_directory'])
        card.update(build_mode='script' if build and build.get('mode') == 'script' else 'none',
                    build_source=str(build.get('source') or '') if build else '',
                    build_script=str(build.get('script') or '') if build else '')
        result.append(card)
    return result


def copyable_limiter_configuration(directory):
    """Return one editable limiter invocation with the resource that supplies it."""
    data = decisions(directory, check_paths=False)
    invocation = str(data.get('limiter') or '').strip()
    base = BASE
    if data.get('limiter_mode') == 'existing':
        template = expand_repo_root(data.get('limiter_file', '')) or path_at(data.get('limiter_file', ''), directory)
        invocation = template.read_text(encoding='utf-8').strip()
        base = template.parent
    lexer = shlex.shlex(invocation, posix=True)
    lexer.whitespace_split = True
    lexer.commenters = ''
    executable = lexer.get_token()
    if not executable:
        raise ValueError('This workflow has no limiter invocation to copy.')
    limiter_path = expand_repo_root(executable) or path_at(executable, base)
    invocation = shlex.quote(str(limiter_path)) + ' ' + invocation[lexer.instream.tell():].lstrip()
    matches = []
    for entry in data.get('resources', []):
        root = expand_repo_root(entry.get('root', '')) or path_at(entry.get('root', ''), BASE)
        if limiter_path.is_relative_to(root):
            matches.append((entry.get('role') == 'limiter', len(root.parts), entry, root))
    if not matches:
        raise ValueError('This workflow has no build resource containing its limiter executable.')
    _, _, entry, root = max(matches, key=lambda item: item[:2])
    resource = {key: str(entry.get(key) or '') for key in ('mode', 'script', 'commands', 'source', 'artifact')}
    resource['root'] = str(root)
    for key in ('script', 'source'):
        if resource[key]:
            resource[key] = str(expand_repo_root(resource[key]) or path_at(resource[key], BASE))
    if resource['mode'] in {'script', 'commands'} and not resource['artifact']:
        resource['artifact'] = str(limiter_path.relative_to(root))
    return {'invocation': invocation, 'resource': resource}


def duplicate(source, name):
    """Copy a workflow definition without stale submission output."""
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Workflow name: use lowercase letters, numbers and hyphens.')
    source_data = decisions(source)
    destination = USER_RUNS / name
    if destination.exists():
        raise ValueError(f'Workflow already exists: {destination}')
    destination.mkdir(parents=True)
    try:
        copied = 0
        for path in source.iterdir():
            if path.name in {'submit.sh', 'submit.sh.files', 'source'} or (path.name.startswith('.') and path.name != '.slurmy-workflow.json'):
                continue
            if path.is_file():
                shutil.copy2(path, destination / path.name)
                copied += 1
            elif path.is_dir():
                shutil.copytree(path, destination / path.name)
                copied += 1
        if not copied or not (destination / 'Makefile').is_file():
            raise ValueError('The source does not contain a duplicable workflow definition.')
        makefile = destination / 'Makefile'
        makefile.write_text(re.sub(r'(?m)^REPO_ROOT\s*:?=.*$', 'REPO_ROOT := ../../..',
                                   makefile.read_text(encoding='utf-8'), count=1), encoding='utf-8')
        def remap(value):
            if isinstance(value, str) and (value == str(source) or value.startswith(str(source) + '/')):
                return str(destination) + value[len(str(source)):]
            if isinstance(value, list):
                return [remap(item) for item in value]
            if isinstance(value, dict):
                return {key: remap(item) for key, item in value.items()}
            return value
        data = remap(source_data)
        data['name'] = name
        data.pop('_previous_directories', None)
        (destination / '.slurmy-workflow.json').write_text(
            json.dumps(data, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        (destination / 'building.txt').write_text(''.join(
            item['root'] + '\n' + (item.get('script', '') if item['mode'] == 'script' else
                                   str(destination / f'build-resource-{index}.sh') if item['mode'] == 'commands' else '') + '\n'
            for index, item in enumerate(data['resources'])), encoding='utf-8')
        (destination / 'resource_limiter_template.txt').write_text(data['limiter'].rstrip() + '\n', encoding='utf-8')
        jobpairs = destination / 'jobpairs.csv'
        if jobpairs.is_file():
            jobpairs.write_text(jobpairs.read_text(encoding='utf-8').replace(str(source), str(destination)), encoding='utf-8')
    except Exception:
        shutil.rmtree(destination)
        raise
    return destination
