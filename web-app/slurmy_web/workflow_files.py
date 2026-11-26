"""Replace generated definitions through the same validator as the workflow editor."""
from __future__ import annotations

import json
import re
import shlex
import shutil
import tempfile
from pathlib import Path

from slurmy import read_builds
from . import workflows
from .workflow_paths import ALIASES, workflow_file, canonical_name

FILES = ('workflow.json', 'solver-problem-pairs.csv', 'solvers.csv', 'problem-globs.txt', 'problem-selection.json', 'axiom-globs.txt',
         'building.json', 'resource-limiter.txt', 'Makefile')
SETTINGS = {'GET_COMPUTER_INFO': 'get_computer_info', 'BATCH_SIZE': 'batch_size', 'PAIR_ORDER': 'pair_order',
            'SLURMY_MAX_OUTSTANDING': 'max_outstanding', 'SLURMY_HOST': 'host',
            'SLURMY_PARTITION': 'partition', 'SLURMY_RESERVATION': 'reservation', 'SLURMY_ACCOUNT': 'account'}
ASSIGNMENTS = re.compile(r'(?m)^(' + '|'.join([*SETTINGS, 'REPO_ROOT', 'AXIOMS']) + r')[ \t]*(?::|\?)?=[ \t]*([^\n]*)$')


def definition_file(directory, name):
    name = canonical_name(name)
    if name not in FILES:
        raise ValueError('Choose one of the generated workflow files.')
    path = workflow_file(directory, name)
    if not path.is_file() or path.is_symlink():
        raise ValueError('Generated workflow file not found, or is a symbolic link.')
    return path


def makefile_commands(body):
    return '\n'.join(line.rstrip() for line in ASSIGNMENTS.sub('', body).splitlines()
                     if line.strip() and not line.lstrip().startswith('#'))


def makefile_settings(data, current, replacement):
    if makefile_commands(current) != makefile_commands(replacement):
        raise ValueError('Makefile replacement can change generated settings, but custom commands or rules cannot be represented in the workflow editor.')
    found = {}
    for match in ASSIGNMENTS.finditer(replacement):
        key, value = match.groups()
        if key in found:
            raise ValueError(f'Duplicate Makefile setting: {key}')
        found[key] = value.strip()
    # Missing assignments have the same defaults as the generated workflow.
    defaults = dict(batch_size='1', pair_order='solver-major', max_outstanding='50',
                    host='datalab', partition='CPU-amd', reservation='', account='', get_computer_info='true')
    for key, field in SETTINGS.items():
        data[field] = found.get(key, defaults[field])
    if data['get_computer_info'].lower() not in {'true', 'false', '1', '0', 'yes', 'no'}:
        raise ValueError('GET_COMPUTER_INFO must be true or false.')
    data['get_computer_info'] = data['get_computer_info'].lower() in {'true', '1', 'yes'}
    if 'AXIOMS' in found and found['AXIOMS'] != 'axiom-globs.txt':
        raise ValueError('Use axiom-globs.txt to replace custom axiom patterns.')
    if data.get('axiom_mode') == 'custom' and 'AXIOMS' not in found:
        data.update(axiom_mode='cluster', axiom_globs=[])


def replace(directory, name, content, progress=None):
    source = definition_file(directory, name)
    name = canonical_name(name)
    if directory.parent != workflows.USER_RUNS.resolve():
        raise ValueError('Duplicate an example before replacing its files.')
    if not isinstance(content, str) or '\x00' in content:
        raise ValueError('Upload a UTF-8 text file without NUL characters.')
    content = content.removeprefix('\ufeff')
    data = workflows.decisions(directory, check_paths=False)
    # Parse uploaded relative paths with the workflow directory as their base.
    with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=directory,
                                     prefix='.replace-', suffix=Path(name).suffix) as upload:
        upload.write(content); upload.flush()
        temporary = Path(upload.name)
        if name == 'workflow.json':
            replacement=json.loads(content)
            if not isinstance(replacement,dict): raise ValueError('Workflow definition must be a JSON object.')
            if replacement.get('name',directory.name)!=directory.name: raise ValueError('Use the workflow rename control to change its name.')
            replacement.update(name=directory.name,host=data.get('host','datalab'))
            data=replacement
        elif name == 'solver-problem-pairs.csv':
            data.update(mode='jobpairs', jobpairs=str(temporary), configurations=[], configurations_file='', globs=[])
        elif name == 'problem-selection.json':
            selections=json.loads(content)
            if not isinstance(selections,list):raise ValueError('Problem selections must be a JSON list, aligned with problem sources.')
            data['problem_selections']=selections
        elif name == 'solvers.csv':
            data.update(mode='interactive', configurations=workflows.imported_rows(temporary, True), configurations_file='')
        elif name in {'problem-globs.txt', 'axiom-globs.txt'}:
            patterns = [line.strip() for line in content.splitlines() if line.strip()]
            patterns = [value if value.startswith('tptp://') else str(workflows.path_at(value, directory)) for value in patterns]
            if name == 'problem-globs.txt':
                if patterns!=data.get('globs'):data['problem_selections']=[]
                data['globs'] = patterns
            else:
                data.update(axiom_globs=patterns, axiom_mode='custom' if patterns else 'cluster')
        elif name == 'building.json':
            builds = read_builds(temporary)
            data.update(building_mode='create', building_file='', resources=[dict(
                root=str(build.root), source=str(build.source) if build.source else '',
                mode='script' if build.recipe else 'none', script=shlex.join([str(build.recipe),*build.arguments]) if build.recipe else '',
                artifact=build.artifact or '', build_policy=build.build_policy) for build in builds])
            data = workflows.resource_roles(data)
        elif name == 'resource-limiter.txt':
            lexer = shlex.shlex(content.strip(), posix=True)
            lexer.whitespace_split = True; lexer.commenters = ''
            executable = lexer.get_token()
            if not executable:
                raise ValueError('The limiter template is empty.')
            limiter = shlex.quote(str(workflows.path_at(executable, directory))) + ' ' + content.strip()[lexer.instream.tell():]
            data.update(limiter_mode='inline', limiter_file='', limiter=limiter.strip())
        elif name == 'Makefile':
            makefile_settings(data, source.read_text(), content)
        # No published files change until the editor's full validation passes.
        _, files, count = workflows.specification(data, directory, progress)
        if name == 'Makefile' and makefile_commands(content) != makefile_commands(files['Makefile']):
            raise ValueError('This Makefile contains custom rules that the workflow editor cannot regenerate. Its files were kept unchanged.')
        if name == 'solver-problem-pairs.csv':
            data['jobpairs'] = str(directory / 'solver-problem-pairs.csv')
            files['workflow.json'] = json.dumps({key:value for key,value in data.items() if key!='host'}, indent=2, sort_keys=True) + '\n'
    changed = set(FILES) | set(ALIASES.values()) | set(files) | {'submit.sh'}
    with tempfile.TemporaryDirectory(prefix='.replace-backup-', dir=directory) as folder:
        backup = Path(folder)
        existing = set()
        for filename in changed:
            target = directory / filename
            if target.is_symlink() or (target.exists() and not target.is_file()):
                raise ValueError(f'Cannot replace a symbolic link or non-file: {filename}')
            if target.is_file():
                shutil.copy2(target, backup / filename); existing.add(filename)
        try:
            for filename, body in files.items():
                target = directory / filename
                stage = target.with_name('.' + target.name + '.slurmy-new')
                stage.write_text(body, encoding='utf-8'); stage.replace(target)
                if target.suffix == '.sh': target.chmod(0o755)
            # Drop obsolete derivatives and prevent dispatching stale prepared input.
            for filename in set(FILES) | set(ALIASES.values()) | {'submit.sh'}:
                if filename not in files:
                    (directory / filename).unlink(missing_ok=True)
        except BaseException:
            for filename in files:
                (directory / ('.' + filename + '.slurmy-new')).unlink(missing_ok=True)
            for filename in changed:
                target = directory / filename
                if filename in existing: shutil.copy2(backup / filename, target)
                else: target.unlink(missing_ok=True)
            raise
    return directory, count
