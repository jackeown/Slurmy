"""Self-contained workflow archives. Import never executes bundled commands."""
from __future__ import annotations
from ssh_transport import connection_options


from dataclasses import asdict
import base64
import glob
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess
import tarfile
import tempfile
import time

from slurmy import order_jobpairs, path_at, read_pairs, read_builds, read_axioms
from . import workflows
from slurmy_archives import tptp_collection_root, TPTP_DEFAULT_ROOT

FORMAT = 'slurmy-workflow-v1'
MAX_EXPANDED = 30 * 1024**3


def safe_member(member):
    path = PurePosixPath(member.name)
    if (path.is_absolute() or '..' in path.parts or '\\' in member.name or
            not path.parts or not (member.isfile() or member.isdir())):
        raise ValueError('Workflow archive contains an unsafe path or unsupported file type.')
    return path


def add_tree(archive, source, name, ancestors=frozenset()):
    """Materialize symlinks so the archive has no external file dependencies."""
    source = Path(source)
    real = source.resolve(strict=True)
    if real in ancestors:
        raise ValueError(f'Circular symlink in workflow resource: {source}')
    if source.is_dir():
        info = tarfile.TarInfo(name + '/')
        info.type = tarfile.DIRTYPE
        info.mode = 0o755
        archive.addfile(info)
        for child in sorted(source.iterdir()):
            if child.name in {'.git', '__pycache__', 'submit.sh.files'}:
                continue
            add_tree(archive, child, name + '/' + child.name, ancestors | {real})
    elif source.is_file():
        info = archive.gettarinfo(str(source), arcname=name)
        info.type = tarfile.REGTYPE
        info.linkname = ''
        info.size = source.stat().st_size
        info.mode = source.stat().st_mode & 0o777
        with source.open('rb') as stream:
            archive.addfile(info, stream)
    else:
        raise ValueError(f'Unsupported resource file: {source}')


def collect_rows(directory, data, host):
    if data['mode'] == 'jobpairs':
        return workflows.imported_rows(path_at(data['jobpairs'], directory))
    configs = (data.get('configurations') if data['mode'] == 'interactive' else
               workflows.imported_rows(path_at(data['configurations_file'], directory), True))
    problems = set()
    for pattern in data.get('globs', []):
        problems.update(workflows.problem_glob(pattern, host=host)[1])
    if not configs or not problems:
        raise ValueError('Workflow has no solver configurations or problems.')
    return order_jobpairs([{**config, 'problem': problem} for config in configs
                          for problem in sorted(problems)], data.get('pair_order') or 'solver-major')


def export_bundle(directory, host):
    data = workflows.decisions(directory)
    rows = collect_rows(directory, data, host)
    rows = [{**row, **workflows.effective_postprocessor(row, data, directory, check_paths=True)} for row in rows]
    global_post = workflows.workflow_postprocessor(data, directory)
    builds = read_builds(directory / 'building.txt')
    limiter = (directory / 'resource_limiter_template.txt').read_text().strip()
    if not Path(shlex.split(limiter)[0]).is_absolute():
        executable = shlex.split(limiter)[0]
        limiter = shlex.quote(str(path_at(executable, directory))) + limiter[len(shlex.quote(executable)):]
    paths = set()
    for build in builds:
        paths.update(p for p in (build.root, build.source, build.recipe) if p is not None)
    paths.update(path_at(row['solver_directory'], directory) for row in rows)
    command_assets = set()
    for row in [*rows, {'postprocessor_' + key: value for key, value in global_post.items()}]:
        row.update(workflows.postprocessor_settings(row, directory))
        if row['postprocessor_mode'] == 'custom':
            paths.add(Path(row['postprocessor_directory']))
            for word in shlex.split(row['postprocessor_command']):
                value = word.split('=', 1)[-1]
                candidate = Path(value)
                if candidate.is_absolute() and candidate.exists() and not value.startswith(('/usr/bin/', '/bin/')):
                    paths.add(candidate); command_assets.add(str(candidate))
    remote = set()
    for row in rows:
        row['solver_directory'] = str(path_at(row['solver_directory'], directory))
        if row['problem'].startswith('tptp:'):
            remote.add(row['problem'][5:])
        else:
            row['problem'] = str(path_at(row['problem'], directory))
            paths.add(Path(row['problem']))
    axiom_file = directory / 'axiom-globs.txt'
    axioms = []
    if axiom_file.is_file():
        sources, destinations = read_axioms(axiom_file, host=host)
        paths.update(source for source in sources if not str(source).startswith('tptp:'))
        remote.update(str(source)[5:] for source in sources if str(source).startswith('tptp:'))
        axioms = [{'source': str(source)[5:] if str(source).startswith('tptp:') else str(source), 'relative': str(destinations[source])} for source in sources]
    local_includes = []
    if not axioms:
        for row in rows:
            if (not row['problem'].startswith('tptp:') and not
                    ('TPTP=' in row['command'] and any((build.root/'Axioms').is_dir() for build in builds))):
                text = Path(row['problem']).read_text(encoding='utf-8', errors='replace')
                text = re.sub(r'/\*.*?\*/|%[^\n]*', '', text, flags=re.S)
                local_includes.extend(re.findall(r'''\binclude\s*\(\s*['"]([^'"]+)['"]''', text))
    remote_roots = set()
    for problem in remote:
        root = tptp_collection_root(problem)
        if not root and '/Axioms/' in problem:
            root = tptp_collection_root(problem.replace('/Axioms/', '/Problems/', 1))
        if not root:
            raise ValueError('Unsupported shared problem path.')
        remote_roots.add(root)
    shared_roots = remote_roots if not axioms else set()
    payload = dict(postprocessor=global_post,command_assets=sorted(command_assets),problem_order=data.get('_problem_order',[]),format=FORMAT, name=directory.name, rows=rows, editor_mode=data['mode'], pair_order=data.get('pair_order') or 'solver-major',
                   builds=[{k: str(v) if isinstance(v, Path) else v for k, v in asdict(build).items()} for build in builds],
                   limiter=limiter, axioms=axioms, shared_roots=sorted(shared_roots), default_tptp_root=TPTP_DEFAULT_ROOT,
                   settings={key: data.get(key, default) for key, default in
                             [('partition', 'CPU-amd'), ('reservation', ''), ('account', ''),
                              ('batch_size', 1), ('max_outstanding', 50), ('get_computer_info', True)]})
    output = tempfile.TemporaryFile()
    try:
        with tempfile.TemporaryDirectory(prefix='slurmy-export-') as tmp:
            remote_archive = Path(tmp) / 'shared.tar'
            # Only selected problems and their version's axiom library are read remotely.
            request = base64.b64encode(json.dumps(dict(problems=sorted(remote), roots=sorted(remote_roots),
                                                       local_includes=local_includes, include_shared=not bool(axioms),
                                                       default_root=TPTP_DEFAULT_ROOT)).encode()).decode()
            script = r'''import base64,json,os,re,sys,tarfile
spec=json.loads(base64.b64decode(sys.argv[1]))
roots=spec['roots']
def root_for(path):
 for root in roots:
  if path.startswith(root+'/'): return root
 raise SystemExit('Invalid shared problem path')
queue=[(path,root_for(path)) for path in spec['problems']]
default=spec['default_root']
for include in spec['local_includes']:
 queue.append((os.path.join(default,include),default))
roots=set(roots)|({default} if spec['local_includes'] else set())
seen=set()
with tarfile.open(fileobj=sys.stdout.buffer,mode='w|',dereference=True) as archive:
 for root in sorted(roots) if spec['include_shared'] else []:
  if not (root.startswith(('/share/datasets/TPTP/','/share/TPTP/')) or root=='/share/Slurmy-TPTP-v9.2.1') or '..' in root.split('/'):
   raise SystemExit('Invalid shared root')
  directory=tarfile.TarInfo('assets/'+root.lstrip('/')+'/Axioms/');directory.type=tarfile.DIRTYPE;directory.mode=0o755
  archive.addfile(directory)
 while queue:
  path,root=queue.pop()
  if path in seen:continue
  seen.add(path)
  if not os.path.realpath(path).startswith(os.path.realpath(root)+'/'):
   raise SystemExit('Include escapes its TPTP collection')
  if not os.path.isfile(path):raise SystemExit('Required shared input is missing: '+path)
  archive.add(path,arcname='assets/'+path.lstrip('/'),recursive=False)
  with open(path,encoding='utf-8',errors='replace') as source: text=source.read()
  text=re.sub(r'/\*.*?\*/|%[^\n]*','',text,flags=re.S)
  for include in re.findall(r"\binclude\s*\(\s*['\"]([^'\"]+)['\"]",text):
   if spec['include_shared']:queue.append((os.path.join(root,include),root))
'''
            if remote or local_includes:
                with remote_archive.open('wb') as target:
                    result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10',
                                             '--', host, 'python3', '-', request], input=script.encode(),
                                            stdout=target, stderr=subprocess.PIPE, timeout=1800)
                if result.returncode:
                    raise ValueError('Could not bundle shared TPTP inputs: ' + result.stderr.decode(errors='replace'))
            roots = sorted(paths, key=str)
            roots = [p for p in roots if not any(parent in paths for parent in p.parents)]
            with tarfile.open(fileobj=output, mode='w:gz') as archive:
                for path in roots:
                    add_tree(archive, path, 'assets/' + str(path).lstrip('/'))
                if remote_archive.exists():
                    with tarfile.open(remote_archive) as shared:
                        for member in shared:
                            safe_member(member)
                            archive.addfile(member, shared.extractfile(member) if member.isfile() else None)
                encoded = json.dumps(payload, indent=2).encode()
                info = tarfile.TarInfo('workflow.json'); info.size = len(encoded); info.mode = 0o600
                archive.addfile(info, io.BytesIO(encoded))
        output.seek(0)
        return output
    except BaseException:
        output.close()
        raise


def command_paths(command, mapping):
    """Relocate complete shell path words, retaining normal shell quoting."""
    replacements = {}
    for old, new in sorted(mapping.items(), key=lambda item: len(item[0]), reverse=True):
        if old not in command:
            continue
        # Quoted paths may contain spaces; unquoted path suffixes end at shell separators.
        for quote in ("'", '"', ''):
            suffix = r"(?:/[^'\n]*)?" if quote == "'" else r'(?:/[^"\n]*)?' if quote else r'''(?:/[^\s;|&()'"\n]*)?'''
            pattern = r'(?<![^\s;|&()=])' + re.escape(quote + old) + '(' + suffix + ')' + re.escape(quote) + r'(?=$|[\s;|&()])'
            def replacement(match):
                token = f'\x00portable{len(replacements)}\x00'
                replacements[token] = shlex.quote(new + (match[1] or ''))
                return token
            command = re.sub(pattern, replacement, command)
    for token, replacement in replacements.items():
        command = command.replace(token, replacement)
    return command


def import_bundle(stream, name, selected_host=None):
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]*', name):
        raise ValueError('Workflow name: use lowercase letters, numbers and hyphens.')
    destination = workflows.USER_RUNS / name
    if destination.exists():
        raise ValueError('A workflow with that name already exists.')
    workflows.USER_RUNS.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.import-', dir=workflows.USER_RUNS) as tmp:
        staging = Path(tmp)
        total = 0; seen = set()
        with tarfile.open(fileobj=stream, mode='r:*') as archive:
            for member in archive:
                relative = safe_member(member)
                if relative.parts[0] not in {'assets', 'workflow.json'} or (relative.parts[0] == 'workflow.json' and len(relative.parts) != 1):
                    raise ValueError('Not a portable Slurmy workflow archive.')
                if str(relative) in seen:
                    raise ValueError('Duplicate path in workflow archive.')
                seen.add(str(relative)); total += member.size
                if total > MAX_EXPANDED:
                    raise ValueError('Expanded workflow exceeds the 30 GiB import limit.')
                if str(relative) == 'workflow.json' and member.size > 128 * 1024**2:
                    raise ValueError('Workflow definition is too large.')
                target = staging.joinpath(*relative.parts)
                if member.isdir():target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as source, target.open('xb') as output:
                        shutil.copyfileobj(source, output)
                    target.chmod(member.mode & 0o777)
        payload = json.loads((staging / 'workflow.json').read_text())
        if payload.get('format') != FORMAT:
            raise ValueError('Unsupported workflow archive version.')
        def local(value):
            if not isinstance(value, str) or not value.startswith('/') or '..' in PurePosixPath(value).parts:
                raise ValueError('Invalid resource path in workflow definition.')
            relative = Path('assets') / value.lstrip('/')
            if not (staging / relative).exists():
                raise ValueError(f'Workflow archive is missing a required input: {value}')
            return str(destination / relative)
        builds = payload['builds']; rows = payload['rows']
        if not isinstance(rows, list) or not rows:
            raise ValueError('Workflow archive has no solver-problem pairs.')
        mapping = {}
        for build in builds:
            for key in ('root', 'source', 'recipe'):
                if build.get(key):mapping[build[key]] = local(build[key])
        shared_roots = payload.get('shared_roots', [])
        for root in shared_roots:
            mapping[root] = str(destination / 'assets' / root.lstrip('/'))
            local(root + '/Axioms')
        default_root = payload.get('default_tptp_root', '/share/TPTP/TPTP-v9.2.1')
        if not isinstance(default_root, str) or tptp_collection_root(default_root + '/Problems/example.p') != default_root:
            raise ValueError('Invalid default TPTP root in workflow archive.')
        default_relative = Path('assets') / default_root.lstrip('/')
        default_axioms = staging / default_relative / 'Axioms'
        if default_axioms.is_dir():
            mapping[default_root] = str(destination / default_relative)
        for asset in payload.get('command_assets', []):
            mapping[asset] = local(asset)
        global_post = workflows.workflow_postprocessor({'postprocessor': payload.get('postprocessor', {})}, check_paths=False)
        if global_post['mode'] == 'custom':
            old_post = global_post['directory']
            global_post['directory'] = local(old_post)
            mapping[old_post] = global_post['directory']
        normalized = []
        for row in rows:
            row = dict(row)
            row.setdefault('postprocessor_override', True)
            row.update(workflows.postprocessor_settings(row, check_paths=False))
            if row['postprocessor_mode'] == 'custom':
                old_post = row['postprocessor_directory']
                row['postprocessor_directory'] = local(old_post)
                mapping[old_post] = row['postprocessor_directory']
            old = row['problem'].removeprefix('tptp:')
            row['problem'] = local(old); mapping[old] = row['problem']
            old_solver = row['solver_directory']; row['solver_directory'] = local(old_solver)
            mapping[old_solver] = row['solver_directory']
            normalized.append(row)
        global_post['command'] = command_paths(global_post['command'], mapping)
        relocated_commands = {}
        for row in normalized:
            command = row['command']
            if command not in relocated_commands:
                relocated_commands[command] = command_paths(command, mapping)
            row['command'] = relocated_commands[command]
            if row['postprocessor_mode'] == 'custom':
                row['postprocessor_command'] = command_paths(row['postprocessor_command'], mapping)
        resources = []
        for build in builds:
            resources.append({**build, 'legacy': bool(build.get('recipe') and not build.get('artifact')), **{key: local(build[key]) if build.get(key) else '' for key in ('root','source','recipe')}})
        for root in shared_roots:
            resources.append(dict(root=mapping[root], source='', recipe='', artifact=''))
        # Standalone command inputs need declared runtime roots for core upload.
        declared = [Path(build['root']) for build in resources]
        post_roots = [Path(row['postprocessor_directory']) for row in normalized if row['postprocessor_mode'] == 'custom']
        if global_post['mode'] == 'custom':
            post_roots.append(Path(global_post['directory']))
        for asset in payload.get('command_assets', []):
            path = Path(mapping[asset]); root = path if (staging / path.relative_to(destination)).is_dir() else path.parent
            if not any(path.is_relative_to(parent) for parent in [*declared, *post_roots]):
                resources.append(dict(root=str(root), source='', recipe='', artifact=''))
                declared.append(root)
        limiter = command_paths(payload['limiter'], mapping)
        # Each imported shared problem needs its own collection version's includes.
        for row, original in zip(normalized, rows):
            if original['problem'].startswith('tptp:') and shared_roots:
                problem = original['problem'][5:]
                root = next((root for root in shared_roots if problem.startswith(root + '/')), None)
                if root:
                    row['command'] = 'export TPTP; TPTP=$(printf %s ' + shlex.quote(mapping[root]) + '); ' + row['command']
        axioms = payload.get('axioms', [])
        axiom_lines = []
        if axioms:
            for item in axioms:
                relative = PurePosixPath(item['relative'])
                if relative.is_absolute() or '..' in relative.parts:
                    raise ValueError('Invalid axiom destination.')
                source = staging / 'assets' / item['source'].lstrip('/')
                local(item['source'])
                target = staging / 'portable-tptp/Axioms' / str(relative)
                target.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(source, target)
            # Use exact paths so an absent suffix is not treated as a failed glob.
            axiom_lines = [str(destination / file.relative_to(staging)) for file in (staging/'portable-tptp/Axioms').rglob('*') if file.is_file()]
        elif default_axioms.is_dir() and not shared_roots:
            axiom_lines = [str(destination / default_relative / 'Axioms/**/*')]
        settings = payload['settings']
        host = workflows.alias(settings.get('host') or 'datalab'); partition = workflows.alias(settings.get('partition') or 'CPU-amd')
        reservation = workflows.alias(settings['reservation']) if settings.get('reservation') else ''
        account = workflows.alias(settings['account']) if settings.get('account') else ''
        if selected_host is not None:
            from .accounts import memberships, choose
            host = workflows.alias(selected_host)
            # Resolve the portable preference against the importing user's memberships.
            available = memberships(host)['accounts']
            account = choose(available, account or None)
        computer_info = settings.get('get_computer_info', True)
        if not isinstance(computer_info, bool):
            raise ValueError('Invalid computer information setting in workflow archive.')
        batch = workflows.positive(str(settings.get('batch_size', 1)), 'batch size')
        maximum = workflows.positive(str(settings.get('max_outstanding', 50)), 'submitted tasks')
        pair_order = payload.get('pair_order') or 'solver-major'
        if pair_order not in workflows.PAIR_ORDERS:
            raise ValueError('Invalid solver-problem pair order in workflow archive.')
        (staging/'jobpairs.csv').write_text(workflows.csv_text(normalized))
        (staging/'building.txt').write_text(json.dumps(resources, indent=2))
        (staging/'resource_limiter_template.txt').write_text(limiter + '\n')
        # Validate local definitions before publication, without SSH or execution.
        validation_rows = [{**row, 'problem': row['problem'].replace(str(destination), str(staging), 1),
                            'solver_directory': row['solver_directory'].replace(str(destination), str(staging), 1),
                            'postprocessor_directory': row.get('postprocessor_directory', '').replace(str(destination), str(staging), 1),
                            'postprocessor_command': row.get('postprocessor_command', '').replace(str(destination), str(staging))}
                           for row in normalized]
        check_pairs = staging/'.validate-pairs.csv'
        check_pairs.write_text(workflows.csv_text(validation_rows))
        check_builds = staging/'.validate-builds.json'
        check_builds.write_text(json.dumps([{**build, **{key: build[key].replace(str(destination), str(staging), 1)
                            for key in ('root','source','recipe') if build.get(key)}} for build in resources]))
        read_pairs(check_pairs); read_builds(check_builds)
        check_pairs.unlink();check_builds.unlink()

        repo = os.path.relpath(workflows.REPO, destination)
        makefile = f'REPO_ROOT := {repo}\nBATCH_SIZE := {batch}\nPAIR_ORDER := {pair_order}\nGET_COMPUTER_INFO := {str(computer_info).lower()}\nSLURMY_MAX_OUTSTANDING := {maximum}\nSLURMY_PARTITION := {partition}\nSLURMY_RESERVATION := {reservation}\nSLURMY_ACCOUNT := {account}\n'
        if axiom_lines:
            (staging/'axiom-globs.txt').write_text('\n'.join(axiom_lines)+'\n');makefile += 'AXIOMS := axiom-globs.txt\n'
        makefile += 'include $(REPO_ROOT)/implementation/templates/workflow.mk\n'
        (staging/'Makefile').write_text(makefile)
        data = dict(postprocessor=global_post,get_computer_info=computer_info, name=name, host=host, partition=partition, reservation=reservation, account=account, batch_size=batch, max_outstanding=maximum, pair_order=pair_order,
                    mode='jobpairs', jobpairs=str(destination/'jobpairs.csv'), building_mode='existing', building_file=str(destination/'building.txt'),
                    limiter_mode='inline', limiter=limiter, axiom_mode='custom' if axiom_lines else 'cluster', axiom_globs=axiom_lines, _created_at=time.time())
        if payload.get('editor_mode') in {'interactive', 'configurations'} and len(shared_roots) <= 1:
            configs = []
            seen_configs = set()
            for row in normalized:
                config = {key: value for key, value in row.items() if key != 'problem'}
                identity = json.dumps(config, sort_keys=True)
                if identity not in seen_configs:
                    configs.append(config); seen_configs.add(identity)
            data.update(mode='interactive', configurations=configs,
                        globs=sorted({glob.escape(row['problem']) for row in normalized}),
                        pair_order=payload.get('pair_order', 'solver-major'), building_mode='create',
                        resources=[dict(root=build['root'], source=build.get('source') or '',
                                        mode='script' if build.get('recipe') else 'none',
                                        script=shlex.join([build['recipe'],*build.get('arguments',[])]) if build.get('recipe') else '', artifact=build.get('artifact') or '',
                                        build_policy=build.get('build_policy', 'always'))
                                   for build in resources])
            data = workflows.resource_roles(data)
        if payload.get('problem_order') and data['mode']=='interactive':
            order=payload['problem_order']
            if not isinstance(order,list) or not all(isinstance(path,str) for path in order):raise ValueError('Invalid problem order in workflow archive.')
            ordered=[local(path.removeprefix('tptp:')) for path in order]
            if set(ordered)!={row['problem'] for row in normalized}:raise ValueError('Portable problem order does not match its solver-problem pairs.')
            data.update(globs=[glob.escape(path) for path in ordered],problem_selections=[Path(path).name for path in ordered],_problem_order=ordered)
        data.pop('host',None)
        (staging/'workflow.json').write_text(json.dumps(data, indent=2))
        # mkdir is the atomic name claim; never replace an existing workflow.
        destination.mkdir()
        try:
            for child in staging.iterdir():shutil.move(str(child), destination/child.name)
        except BaseException:
            shutil.rmtree(destination);raise
    return destination, len(rows)
