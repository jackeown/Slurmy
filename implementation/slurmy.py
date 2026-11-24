#!/usr/bin/env python3
"""Prepare an explicit table of solver-problem pairs for SLURM; Python runs locally only."""
from __future__ import annotations
import solver_environment

import argparse
import base64
import csv
from dataclasses import dataclass
import hashlib
import io
import json
import math
import os
from pathlib import Path, PurePosixPath
import random
import re
import shlex
import shutil
import sys
import tarfile
import szs
from analyze_run import parse_columns, workflow_script, TIMEOUT_SECONDS, KILL_GRACE_SECONDS
from typing import Sequence
from slurmy_archives import expand_axiom_pattern, expand_axiom_source, tptp_collection_root, TPTP_DEFAULT_ROOT, TPTP_ROOT

VERSION = "1.0.0"
COLUMNS = ("solver_name", "command", "solver_directory", "problem", "wc_limit", "cpu_limit", "mem_limit", "cores",
           "threads_per_core", "cpus", "exclusive_cpu", "exclusive_node", "omit_cpu_limit", "omit_wc_limit", "omit_mem_limit", "environment", "analysis_script", "analysis_columns")
RESULT_COLUMNS = ("task_id", "complete", "status", "return_code", "wall_seconds",
                  "cpu_seconds", "user_seconds", "system_seconds", "cpu_usage_percent",
                  "max_virtual_memory_kib", "max_memory_kib", "timed_out", "memory_out",
                  "task_key", "archive", "szs_status", "szs_output")
REPO = Path(__file__).resolve().parents[1]
PLACEHOLDER = re.compile(r"\{\{([a-z_]+)\}\}")
PAIR_ORDERS = ("problem-major", "solver-major", "random")


class SlurmyError(ValueError):
    pass


def order_jobpairs(rows: list[dict], order: str) -> list[dict]:
    """Order solver-problem pairs reproducibly without changing their contents."""
    if order not in PAIR_ORDERS:
        raise SlurmyError(f"solver-problem pair order must be one of: {', '.join(PAIR_ORDERS)}")
    result = list(rows)
    if order == "problem-major":
        problems = {problem:index for index,problem in enumerate(dict.fromkeys(row["problem"] for row in result))}
        result.sort(key=lambda row: problems[row["problem"]])
    elif order == "solver-major":
        configurations = {}
        for row in result:
            key = tuple(row.get(column, "") for column in COLUMNS if column != "problem")
            configurations.setdefault(key, len(configurations))
        result.sort(key=lambda row: configurations[tuple(row.get(column, "") for column in COLUMNS if column != "problem")])
    else:
        random.Random(0).shuffle(result)
    return result


def path_at(value: str, base: Path) -> Path:
    path = Path(os.path.expanduser(value))
    return Path(os.path.abspath(path if path.is_absolute() else base / path))


def analysis_path(value: str, base: Path, check=True) -> Path:
    if not isinstance(value, str):
        raise ValueError('Enter the path to an analysis script.')
    path = path_at(value.strip(), base)
    if path.suffix not in {'.py', '.sh'}:
        raise ValueError('Choose a Python (.py) or Bash (.sh) analysis script.')
    if check and not path.is_file():
        raise ValueError(f'Analysis script does not exist: {path}')
    return path


def positive(value: str, name: str) -> int:
    if not re.fullmatch(r"[1-9][0-9]*", str(value)):
        raise SlurmyError(f"{name}: expected a positive integer, got {value!r}")
    return int(value)


def nonnegative(value: str, name: str) -> int:
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)", str(value)):
        raise SlurmyError(f"{name}: expected a nonnegative integer (0 means no limit), got {value!r}")
    return int(value)


def boolean(value: str, name: str) -> bool:
    if value.strip().lower() not in {"", "true", "false"}:
        raise SlurmyError(f"{name}: expected true or false")
    return value.strip().lower() == "true"


def memory_bytes(value: str) -> int:
    match = re.fullmatch(r"(0|[1-9][0-9]*)\s*(MB|MiB|GB|GiB|TB|TiB)?", value, re.I)
    if not match:
        raise SlurmyError(f"invalid mem_limit {value!r}; use 0 for no limit, or e.g. 1200MiB or 2GB")
    factors = {"mb": 10**6, "mib": 2**20, "gb": 10**9, "gib": 2**30,
               "tb": 10**12, "tib": 2**40}
    return int(match[1]) * factors[(match[2] or "MB").lower()]


def read_pairs(filename: Path, host=None, partition=None) -> list[dict]:
    all_topology = None
    tasks = []
    solver_directories={}
    local_problems={}
    analysis_paths = {}
    with filename.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames or []
        required = set(COLUMNS) - {"solver_name", "threads_per_core", "exclusive_cpu", "exclusive_node", "omit_cpu_limit", "omit_wc_limit", "omit_mem_limit", "environment", "analysis_script", "analysis_columns"}
        if len(headers) != len(set(headers)) or not required <= set(headers) or set(headers) - set(COLUMNS):
            raise SlurmyError(f"{filename}: columns must be {', '.join(COLUMNS)}; threads_per_core and exclusivity columns may be omitted")
        legacy_analysis = workflow_script(filename.parent) if 'analysis_script' not in headers else None
        for row in reader:
            label = f"{filename}: row ending at line {reader.line_num}"
            if None in row or any(row.get(key) is None for key in required):
                raise SlurmyError(f"{label}: wrong number of CSV fields")
            command = row["command"].strip()
            if not command or "\x00" in command:
                raise SlurmyError(f"{label}: command is empty or contains NUL")
            solver_name = (row.get("solver_name") or "").strip()
            if len(solver_name) > 80 or any(ord(char) < 32 or ord(char) == 127 for char in solver_name):
                raise SlurmyError(f"{label}: solver_name must be at most 80 characters with no control characters")
            cluster_problem = row["problem"].startswith('tptp:')
            problem = Path(row["problem"][5:]) if cluster_problem else local_problems.setdefault(row["problem"],None)
            if not cluster_problem and problem is None:
                problem=path_at(row["problem"],filename.parent)
                if not problem.is_file():raise SlurmyError(f"{label}: problem does not exist: {problem}")
                local_problems[row["problem"]]=problem
            if cluster_problem and not tptp_collection_root(str(problem)):
                raise SlurmyError(f"{label}: invalid cluster TPTP problem: {problem}")
            if not row["problem"]:
                raise SlurmyError(f"{label}: problem does not exist: {problem}")
            solver_root=solver_directories.get(row["solver_directory"])
            if solver_root is None:
                solver_root=path_at(row["solver_directory"],filename.parent)
                if not row["solver_directory"] or not solver_root.is_dir():raise SlurmyError(f"{label}: solver_directory is not an existing directory: {solver_root}")
                solver_directories[row["solver_directory"]]=solver_root
            task = {"task_id": len(tasks), "command": command, "problem": str(problem),
                    "remote_problem": cluster_problem,
                    "solver_root": str(solver_root),
                    "solver": solver_name or Path(shlex.split(command)[0]).name}
            for key in ("wc_limit", "cpu_limit"):
                task[key] = nonnegative(row[key].strip(), f"{label}: {key}")
            if row['cores'].strip().lower() == 'all':
                if not boolean(row.get('exclusive_cpu') or '', f'{label}: exclusive_cpu'):
                    raise SlurmyError(f'{label}: All cores requires dedicated whole CPU sockets.')
                if all_topology is None:
                    from check_threads import dedicated_core_topology
                    all_topology = dedicated_core_topology(host or os.environ.get('SLURMY_HOST', 'datalab'),
                                                          partition or os.environ.get('SLURMY_PARTITION', 'CPU-amd'))
                sockets, per_socket = all_topology
                task['cores'] = per_socket * (sockets if boolean(row.get('exclusive_node') or '', f'{label}: exclusive_node') else 1)
            else:
                task['cores'] = positive(row['cores'].strip(), f'{label}: cores')
            task["threads_per_core"] = positive((row.get("threads_per_core") or "1").strip(),
                                                 f"{label}: threads_per_core")
            task["cpus"] = 0 if row.get("cpus", "").strip().lower() in {"", "auto"} else positive(row["cpus"].strip(), f"{label}: cpus")
            task["mem_limit"] = memory_bytes(row["mem_limit"].strip())
            for key in ("exclusive_cpu", "exclusive_node", *LIMIT_OMISSION_FIELDS):
                task[key] = boolean(row.get(key) or "", f"{label}: {key}")
            try:environment=solver_environment.parse(row.get('environment'))
            except ValueError as exc:raise SlurmyError(f'{label}: {exc}') from exc
            if environment:task['environment']=environment
            script = (row.get('analysis_script', str(legacy_analysis or '')) or '').strip()
            if script:
                if script not in analysis_paths:
                    try:
                        analysis_paths[script] = str(analysis_path(script, filename.parent))
                    except ValueError as exc:
                        raise SlurmyError(f'{label}: {exc}') from exc
                task['analysis_script'] = analysis_paths[script]
            try:
                columns = parse_columns(row.get('analysis_columns'))
                if columns and not script:
                    raise ValueError('Choose an analysis script before declaring its columns.')
            except ValueError as exc:
                raise SlurmyError(f'{label}: {exc}') from exc
            if columns:
                task['analysis_columns'] = columns
            task["task_key"] = hashlib.sha256(json.dumps(task, sort_keys=True).encode()).hexdigest()[:16]
            task.setdefault('environment',{})
            tasks.append(task)
    if not tasks:
        raise SlurmyError("jobpairs.csv contains no solver-problem pairs")
    return tasks


@dataclass(frozen=True)
class BuildSpec:
    root: Path
    recipe: Path | None
    source: Path | None = None
    artifact: str | None = None
    build_policy: str = "always"
    arguments: tuple[str, ...] = ()


def read_builds(filename: Path, check_paths: bool = True) -> list[BuildSpec]:
    content = filename.read_text(encoding="utf-8")
    if content.lstrip().startswith('['):
        records = json.loads(content)
        if not isinstance(records, list):
            raise SlurmyError('building.txt JSON must be a list of resources')
        builds = []
        for record in records:
            if not isinstance(record, dict):
                raise SlurmyError('each build resource must be an object')
            root = path_at(record.get('root', ''), filename.parent)
            source = path_at(record['source'], filename.parent) if record.get('source') else None
            recipe = path_at(record['recipe'], filename.parent) if record.get('recipe') else None
            # The bundled limiter is built offline; carry its source even for
            # older workflows whose build resource left the source field blank.
            if source is None and recipe == REPO / 'implementation/build/runsolver/build.sh':
                source = recipe.parent / 'source'
            artifact = record.get('artifact') or None
            if check_paths and (not root.is_dir() or (source and not source.is_dir()) or (recipe and not recipe.is_file())):
                raise SlurmyError(f'build root, source, or recipe does not exist: {record}')
            artifact_path = PurePosixPath(artifact) if artifact else None
            if artifact_path and (artifact_path.is_absolute() or not artifact_path.parts or '..' in artifact_path.parts or '\n' in artifact):
                raise SlurmyError(f'expected artifact must be relative to the runtime root: {artifact}')
            if recipe and not artifact and not record.get('legacy'):
                raise SlurmyError(f'expected artifact is required for build: {root}')
            if recipe and not artifact and source is None:
                source = root
            if any(root == previous.root for previous in builds):
                raise SlurmyError(f'duplicate resource root: {root}')
            policy = record.get("build_policy", "always")
            if policy not in {"always", "changed"}:
                raise SlurmyError("Unknown build policy")
            arguments=record.get("arguments", [])
            if not isinstance(arguments,list) or not all(isinstance(arg,str) and "\x00" not in arg for arg in arguments):
                raise SlurmyError("Build script arguments must be a list of strings")
            builds.append(BuildSpec(root, recipe, source, artifact, policy, tuple(arguments)))
        return builds
    lines = content.splitlines()
    if len(lines) % 2:
        raise SlurmyError("building.txt must have an even number of lines; retain the empty recipe line")
    builds = []
    for i in range(0, len(lines), 2):
        root = path_at(lines[i].strip(), filename.parent)
        if not lines[i].strip() or (check_paths and not root.is_dir()):
            raise SlurmyError(f"building.txt line {i + 1}: resource root is not an existing directory: {root}")
        recipe = path_at(lines[i + 1].strip(), filename.parent) if lines[i + 1].strip() else None
        if check_paths and recipe is not None and not recipe.is_file():
            raise SlurmyError(f"build script does not exist: {recipe}")
        if any(root == previous.root for previous in builds):
            raise SlurmyError(f"duplicate resource root: {root}")
        builds.append(BuildSpec(root, recipe, root, None))
    return builds


def shared_axiom_root(filename: Path | None) -> str | None:
    """A single full-collection glob can use shared includes without copying."""
    if filename is None:
        return None
    patterns = [line.strip() for line in filename.read_text(encoding='utf-8').splitlines() if line.strip()]
    if len(patterns) != 1:
        return None
    match = re.fullmatch(r'tptp://(TPTP-[A-Za-z0-9_.-]+)/Axioms/\*\*/\*\.ax', patterns[0])
    return f'{TPTP_ROOT}/{match[1]}' if match else None


def read_axioms(filename: Path | None, progress=None, host=None) -> tuple[list[Path], dict[Path, Path]]:
    """Expand globs/archives into one job-local Axioms/ directory."""
    if filename is None:
        return [], {}
    if progress:
        progress('phase', 'Finding selected axioms and inspecting archives', 0, 0)
    destinations = {}
    for line in filename.read_text(encoding='utf-8').splitlines():
        if not line.strip():
            continue
        pattern = line.strip() if line.strip().startswith('tptp://') else str(path_at(line.strip(), filename.parent))
        for source, relative in expand_axiom_source(pattern, host or os.environ.get('SLURMY_HOST','datalab'), progress=progress):
            if relative in destinations.values() and destinations.get(source) != relative:
                raise SlurmyError(f'custom axioms would overwrite the same Axioms/{relative}; choose one source')
            destinations[source] = relative
    if not destinations:
        raise SlurmyError('axiom-globs.txt contains no axiom files')
    return sorted(destinations), destinations


def remote_path(path: Path) -> str:
    return '"${JOB_DIR}/rootfs"/' + shlex.quote(str(path).lstrip("/"))


LIMIT_OMISSION_FIELDS = ('omit_cpu_limit', 'omit_wc_limit', 'omit_mem_limit')
# Preserve the original shell text; shlex.split/join would quote away operators
# and substitutions. Quotes and backslash escapes stay part of each word span.
SHELL_WORD = re.compile(r'''(?:[^\s'"\\;&|<>()]+|\\[\s\S]|"(?:\\.|[^"\\])*"|'[^']*')+|[;&|<>()]+''')


def omit_unlimited_arguments(command: str, task: dict) -> str:
    omitted = set()
    for limit, names in [('cpu_limit', ('cpu_limit',)), ('wc_limit', ('wc_limit',)),
                         ('mem_limit', ('mem_limit', 'mem_limit_mib'))]:
        if task[limit] == 0 and task.get('omit_' + limit, False):
            omitted.update(names)
    if not omitted:
        return command
    tokens = list(SHELL_WORD.finditer(command))
    remove = set()
    for index, token in enumerate(tokens):
        placeholders = set(PLACEHOLDER.findall(token.group()))
        if not placeholders & omitted:
            continue
        if placeholders - omitted:
            raise SlurmyError('Cannot omit a solver argument combining an unlimited limit with another placeholder. Split it into separate arguments or edit the command.')
        raw = token.group()
        try:
            word = shlex.split(raw)[0]
        except (ValueError, IndexError):
            raise SlurmyError('Cannot identify the solver argument to omit; edit the command.')
        if word.startswith('-'):
            # --limit={{cpu_limit}}, -t{{cpu_limit}}, '-timeout({{cpu_limit}})'
            remove.add(index)
        elif word in {'{{' + name + '}}' for name in omitted} and index > 0:
            previous = shlex.split(tokens[index-1].group())
            if previous and previous[0].startswith('-') and not PLACEHOLDER.search(tokens[index-1].group()):
                remove.update((index-1, index))
            else:
                raise SlurmyError('Cannot identify the option before an unlimited solver limit. Use --option {{limit}} or --option={{limit}}, or edit the command.')
        else:
            raise SlurmyError('Cannot safely omit this unlimited solver limit inside a shell expression. Put it in a separate command-line option or edit the command.')
        if index == 0 or (index == 1 and 0 in remove):
            raise SlurmyError('Cannot omit the solver executable.')
    for index in sorted(remove, reverse=True):
        token = tokens[index]
        command = command[:token.start()] + command[token.end():]
    return command


def render(command: str, replacements: dict[str, str]) -> str:
    def replace(match: re.Match) -> str:
        if match[1] not in replacements:
            raise SlurmyError(f"unknown placeholder {match[0]}")
        return replacements[match[1]]
    result = PLACEHOLDER.sub(replace, command)
    if "{{" in result or "}}" in result:
        raise SlurmyError(f"unsupported placeholder syntax in {command!r}")
    return result


def relocate(command: str, paths: Sequence[Path]) -> str:
    # Replace complete shell words (including quoted paths). Do not rewrite
    # arbitrary substrings inside scripts, regular expressions or other words.
    for path in sorted(set(paths), key=lambda p: len(str(p)), reverse=True):
        raw = str(path)
        for spelling in dict.fromkeys((shlex.quote(raw), '"' + raw + '"', raw)):
            pattern = r"(?<![^\s;|&()])" + re.escape(spelling) + r"(?=$|[\s;|&()])"
            command = re.sub(pattern, lambda _: remote_path(path), command)
    return command


def shell_array(name: str, values: Sequence[object]) -> str:
    return name + "=(\n" + "\n".join("  " + shlex.quote(str(value)) for value in values) + "\n)\n"


def write_script(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")
    path.chmod(0o755)


def generate(jobpairs: Path, building: Path, limiter_file: Path,
             axioms_file: Path | None = None, batch_size: int = 1, progress=None,
             max_outstanding: int = 50, pair_order: str | None = None,
             materialize_calls: bool = True, host=None, partition=None,
             get_computer_info: bool = True) -> Path:
    tasks = read_pairs(jobpairs, host, partition)
    scripts = {}
    for task in tasks:
        source = task.get('analysis_script')
        if source and source not in scripts:
            scripts[source] = f'analysis-{len(scripts)}{Path(source).suffix}'
    call_buffer = 80 if get_computer_info else 30
    analysis_buffer = TIMEOUT_SECONDS + KILL_GRACE_SECONDS
    positive(str(batch_size), 'batch size')
    positive(str(max_outstanding), 'maximum submitted SLURM tasks')
    if pair_order is not None and pair_order not in PAIR_ORDERS:
        raise SlurmyError("Invalid solver-problem pair order")
    builds = read_builds(building)
    shared_axioms = shared_axiom_root(axioms_file)
    axioms, axiom_destinations = ([], {}) if shared_axioms else read_axioms(axioms_file, progress, host)
    if progress:
        progress('phase', 'Generating submission scripts', 0, 0)
    limiter = limiter_file.read_text(encoding="utf-8").strip()
    if not limiter or "{{solver_command}}" not in limiter:
        raise SlurmyError("resource_limiter_template.txt must contain {{solver_command}}")
    # First word is an executable path, not an arbitrary shell expression.
    words = shlex.split(limiter)
    computer_info_timestamp = '--timestamp' in words
    # Runsolver captures both streams through its native timestamping pipe.
    # Keep independent raw copies without removing stderr from that pipe.
    capture_combined = '--solver-data' in words and '{{solver_log}}' in limiter
    limiter_path = path_at(words[0], limiter_file.parent)
    roots = [build.root for build in builds]
    if not any(limiter_path.is_relative_to(root) for root in roots):
        raise SlurmyError("limiter executable must be inside a building.txt resource root")
    if not limiter_path.is_file() and not any(build.recipe and limiter_path.is_relative_to(build.root) for build in builds):
        raise SlurmyError(f"limiter does not exist and no recipe builds it: {limiter_path}")
    limiter = re.sub(r"^\s*(?:'[^']*'|\"[^\"]*\"|\S+)", lambda _: remote_path(limiter_path), limiter, count=1)
    # Every command runs in its explicitly selected, mirrored solver directory.
    problems = {Path(t["problem"]) for t in tasks if not t['remote_problem']}
    packaged_roots = set(roots + [Path(t['solver_root']) for t in tasks])
    for task in tasks:
        lexer = shlex.shlex(task['command'], posix=True, punctuation_chars=True)
        lexer.whitespace_split = True
        command_paths = [Path(word) for word in lexer
                         if word.startswith('/') and (Path(word) in problems or
                             any(Path(word).is_relative_to(root) for root in packaged_roots))]
        values = {key: str(task[key]) for key in ("wc_limit", "cpu_limit", "cores")}
        if task["cpus"]:
            values["cpus"] = str(task["cpus"])
        values.update(mem_limit=str(math.ceil(task["mem_limit"] / 10**6)),
                      mem_limit_mib=str(math.ceil(task["mem_limit"] / 2**20)),
                      problem=(shlex.quote(task['problem']) if task['remote_problem'] else remote_path(Path(task["problem"]))))
        task["remote_command"] = solver_environment.exports(task["environment"]) + render(relocate(omit_unlimited_arguments(task["command"], task), command_paths), values)
        values["solver_command"] = '/bin/bash "$CALL_DIR/solver.sh"'
        for name in ("watcher_log", "var_file", "solver_log"):
            values[name] = '"${' + name.upper() + '}"'
        task["limiter_command"] = render(limiter, values)
    batches: list[list[dict]] = []
    # Keep each allocation's core count and sharing policy homogeneous. Pairs
    # still run sequentially; compatible requests fill batches even when input
    # order interleaves configurations, rather than making one batch per pair.
    allocation_fields = ('cores', 'threads_per_core', 'cpus', 'exclusive_cpu', 'exclusive_node')
    open_batches = {}
    for task in tasks:
        # Solver identity and per-pair limits do not change the allocation:
        # pairs run sequentially, each with its own command and limiter settings.
        key = tuple(task[field] for field in allocation_fields)
        batch_id = open_batches.get(key)
        if batch_id is None or len(batches[batch_id]) >= batch_size:
            batch_id = len(batches)
            batches.append([])
            open_batches[key] = batch_id
        batches[batch_id].append(task)
        task['batch_id'] = batch_id
    output = jobpairs.parent / "submit.sh"
    assets = jobpairs.parent / "submit.sh.files"
    if output.exists() or assets.exists():
        raise SlurmyError("submit.sh or submit.sh.files already exists; run make clean before regenerating")
    assets.mkdir()
    try:
        if materialize_calls:(assets / "calls").mkdir()
        (assets / "plans").mkdir()
        (assets / "builds").mkdir()
        metadata = {"slurmy_version": VERSION, "workflow_directory": str(jobpairs.parent), "task_count": len(tasks), "batch_count": len(batches),
                    "batch_size": batch_size, "pair_order": pair_order, "local_calls_materialized": materialize_calls,
                    "result_columns": (*RESULT_COLUMNS, 'analysis') if scripts else RESULT_COLUMNS,
                    "planned_batch_tasks": [[t["task_id"] for t in batch] for batch in batches],
                    "batch_policy": "one solver-problem pair at a time per batch; all orderings group by matching core, socket, thread and sharing requests; batches may mix solver configurations",
                    "get_computer_info": get_computer_info,
                    "shared_axiom_root": shared_axioms,
                    "buffers": {"per_call_seconds": call_buffer, "per_call_memory_mib": 128, "batch_seconds": 60}}
        if scripts:
            metadata['buffers']['analysis_seconds'] = analysis_buffer
        (assets / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        (assets / "manifest.jsonl").write_text("".join(json.dumps(t) + "\n" for t in tasks))
        (assets/'call-bundles').mkdir()
        index=0
        for batch_id,batch in enumerate(batches):
            with tarfile.open(assets/'call-bundles'/f'batch_{batch_id:06d}.tar.gz','w:gz',compresslevel=1) as bundle:
                for task in batch:
                    index+=1
                    def emit(name,body,mode=0o755):
                        payload=body.encode('utf-8')
                        member=tarfile.TarInfo(f"{task['task_id']}/{name}")
                        member.size=len(payload);member.mode=mode
                        bundle.addfile(member,io.BytesIO(payload))
                        if materialize_calls:
                            target=assets/'calls'/str(task['task_id'])/name
                            target.write_bytes(payload);target.chmod(mode)
                    call = assets / "calls" / str(task["task_id"])
                    if materialize_calls:call.mkdir()
                    analysis = scripts.get(task.get('analysis_script'), '')
                    if capture_combined or analysis:
                        solver_script = (
                            '#!/usr/bin/env bash\n'
                            '# Tee raw streams while runsolver measures their combined output live.\n'
                            'exec 3> >(tee "$SOLVER_STDOUT_LOG")\nstdout_capture=$!\n'
                            'exec 4> >(tee "$SOLVER_STDERR_LOG" >&2)\nstderr_capture=$!\n'
                            '( ' + task['remote_command'] + ' ) >&3 2>&4\nsolver_status=$?\n'
                            'exec 3>&- 4>&-\n'
                            'wait "$stdout_capture"; stdout_status=$?\n'
                            'wait "$stderr_capture"; stderr_status=$?\n'
                            '(( stdout_status == 0 && stderr_status == 0 )) || exit 74\n'
                            'exit "$solver_status"\n')
                    else:
                        solver_script = ('#!/usr/bin/env bash\n( ' + task['remote_command'] +
                                         ' ) 2> "$SOLVER_STDERR_LOG"\n')
                    emit('solver.sh', solver_script)
                    # Executable bits can be lost when resource trees cross filesystems
                    # (notably when workflows are prepared on macOS and staged on Linux).
                    # Repair the known limiter binary on the cluster immediately before
                    # invocation; if the filesystem is mounted noexec, the subsequent
                    # execution still fails with a useful diagnostic in controller.log.
                    limiter_exec = remote_path(limiter_path)
                    chmod_error = shlex.quote(f"Cannot make resource limiter executable: {limiter_path}")
                    not_exec_error = shlex.quote(
                        f"Resource limiter is not executable (possibly a noexec filesystem): {limiter_path}"
                    )
                    limiter_script = (
                        "#!/usr/bin/env bash\n"
                        f"if [[ ! -x {limiter_exec} ]]; then\n"
                        f"  chmod u+x -- {limiter_exec} || {{ printf '%s\\n' {chmod_error} >&2; exit 126; }}\n"
                        "fi\n"
                        f"[[ -x {limiter_exec} ]] || {{ printf '%s\\n' {not_exec_error} >&2; exit 126; }}\n"
                        + task["limiter_command"] + "\n"
                    )
                    emit("limiter.sh", limiter_script)
                    default_tptp = shared_axioms or (tptp_collection_root(task['problem']) if task['remote_problem'] else None) or TPTP_DEFAULT_ROOT
                    config = "".join(key.upper() + "=" + shlex.quote(str(value)) + "\n" for key, value in {
                        "task_id": task["task_id"], "task_key": task["task_key"], "batch_id": task["batch_id"],
                        "cores": task["cores"], "threads_per_core": task["threads_per_core"],
                        "cpus": task["cpus"], "wc_limit": task["wc_limit"],
                        "cpu_limit": task["cpu_limit"], "mem_limit_mib": math.ceil(task["mem_limit"] / 2**20),
                        "get_computer_info": int(get_computer_info),
                        "analysis_script": analysis,
                        "analysis_columns": json.dumps(task.get('analysis_columns', [])),
                        "computer_info_timestamp": int(computer_info_timestamp),
                            "solver_root_rel": task['solver_root'].lstrip("/"),
                            "tptp_root_rel": '.slurmy-tptp' if axioms else '',
                            "tptp_default_root": shared_axioms or os.environ.get('SLURMY_TPTP_ROOT') or default_tptp}.items())
                    emit("config.sh",config,0o644)
                    if progress and (index % 1000 == 0 or index == len(tasks)):
                        progress('calls', 'Generating solver-problem pair scripts', index, len(tasks))
        for i, batch in enumerate(batches):
            config = shell_array("TASK_IDS", [t["task_id"] for t in batch])
            for name, key in (("TASK_CORES", "cores"), ("TASK_CPUS", "cpus"), ("TASK_WALL", "wc_limit")):
                config += shell_array(name, [t[key] for t in batch])
            config += shell_array("TASK_MEMORY", [math.ceil(t['mem_limit'] / 2**20) for t in batch])
            config += f"BATCH_ID={i}\nEXCLUSIVE_CPU={int(any(t['exclusive_cpu'] for t in batch))}\nEXCLUSIVE_NODE={int(any(t['exclusive_node'] for t in batch))}\n"
            memory = 0 if any(t["mem_limit"] == 0 for t in batch) else len(batch) * (max(math.ceil(t["mem_limit"] / 2**20) for t in batch) + 128)
            config += f"MEMORY_MIB={memory}\n"
            wall = 604800 if any(t["wc_limit"] == 0 for t in batch) else 60 + sum(
                t["wc_limit"] + call_buffer + (analysis_buffer if t.get('analysis_script') else 0) for t in batch)
            config += f"WALL_SECONDS={wall}\n"
            config += shell_array("TASK_THREADS_PER_CORE", [t["threads_per_core"] for t in batch])
            config += f"MAX_CORES={max(t['cores'] for t in batch)}\nMAX_CPUS={max(t['cpus'] for t in batch)}\n"
            config += f"MAX_THREADS_PER_CORE={max(t['threads_per_core'] for t in batch)}\n"
            (assets / "plans" / f"batch_{i:06d}.sh").write_text(config)
            if progress and ((i + 1) % 100 == 0 or i + 1 == len(batches)):
                progress('batches', 'Generating SLURM batch plans', i + 1, len(batches))
        if progress:
            progress('phase', 'Finalizing submission files', 0, 0)
        for name in ("submit.sh", "remote_dispatch.sh", "remote_prepare.sh", "remote_finish_upload.sh", "remote_submit.sh", "batch.sh", "call.sh", "timed_call.sh", "csv.sh", "workflow_name.sh"):
            source = REPO / "implementation/templates" / name
            destination = output if name == "submit.sh" else assets / name
            # A generated file should look newly generated. copy2() preserved the
            # template's old mtime, which made make immediately call it stale.
            shutil.copyfile(source, destination)
            shutil.copymode(source, destination)
        (assets / "submission_settings.sh").write_text(f'MAX_OUTSTANDING_DEFAULT={max_outstanding}\nBUILD_COUNT_DEFAULT={sum(build.recipe is not None for build in builds)}\nLOCAL_PROBLEM_FILES_DEFAULT={len(problems)}\nCLUSTER_PROBLEM_FILES_DEFAULT={len({task["problem"] for task in tasks if task["remote_problem"]})}\n', encoding='utf-8')
        shutil.copyfile(REPO / "implementation/job_names.py", assets / "job_names.py")
        shutil.copyfile(REPO / "implementation/tools/GetComputerInfo", assets / "GetComputerInfo")
        (assets / "szs.sh").write_text(szs.shell_library())
        if scripts:
            for source, name in scripts.items():
                shutil.copyfile(source, assets / name)
            (assets / 'analysis-sources.json').write_text(json.dumps(scripts), encoding='utf-8')
            shutil.copyfile(REPO / 'implementation/analyze_run.py', assets / 'analyze_run.py')
            (assets / 'analysis-settings.sh').write_text(
                f'ANALYSIS_TIMEOUT={TIMEOUT_SECONDS}\nANALYSIS_KILL_GRACE={KILL_GRACE_SECONDS}\n', encoding='utf-8')
        # Reuse the local build driver. Remote recipes build in a disposable
        # copy of the declared root; downloaded files preserve that layout.
        shutil.copy2(REPO / "implementation/build/slurmy-build.py", assets / "builds/driver.py")
        shutil.copy2(REPO / "implementation/submit_coordinator.py", assets / "submit_coordinator.py")
        shutil.copyfile(REPO / "implementation/check_threads.py", assets / "check_threads.py")
        shutil.copyfile(REPO / "implementation/ssh_transport.py", assets / "ssh_transport.py")
        shutil.copyfile(REPO / "implementation/slurm_api.py", assets / "slurm_api.py")
        shutil.copyfile(REPO / "implementation/local_dispatch.py", assets / "local_dispatch.py")
        shutil.copyfile(REPO / "implementation/ssh_transport.py", assets / "builds/ssh_transport.py")
        shutil.copyfile(REPO / "implementation/package_inputs.py", assets / "package_inputs.py")
        if axioms:
            shutil.copy2(REPO / 'implementation/prepare_axioms.py', assets / 'prepare_axioms.py')
            (assets / 'axiom-paths.json').write_text(json.dumps(
                [(str(source), relative.as_posix()) for source, relative in axiom_destinations.items()]), encoding='utf-8')
            write_script(assets / 'prepare_axioms.sh', '#!/usr/bin/env bash\nset -euo pipefail\n'
                         'exec python3 "${JOB_DIR:?}/prepare_axioms.py"\n')
        build_lines = ["#!/usr/bin/env bash", "set -euo pipefail", 'HERE=$(cd -- "$(dirname -- "$0")" && pwd)', 'source "$HERE/../workflow_name.sh"']
        active_builds = [(i, build) for i, build in enumerate(builds) if build.recipe is not None]
        build_total = len(active_builds)
        # A legacy recipe may rely on the previous build's copied tree. Also
        # avoid concurrent writes/reads when one runtime root contains another
        # build's root, source, or recipe. Otherwise the resources are independent.
        parallel_builds = all(build.artifact for _, build in active_builds)
        for i, build in active_builds:
            for j, other in active_builds:
                if i == j:
                    continue
                if any(path.is_relative_to(build.root) for path in
                       (other.root, other.source, other.recipe) if path is not None) \
                   or build.root.is_relative_to(other.root):
                    parallel_builds = False
        commands = []
        for i, build in enumerate(builds):
            if build.recipe is None:
                continue
            wrapper = assets / "builds" / f"recipe_{i}.sh"
            if build.artifact:
                # Use the original script at submission time, so editing a
                # recipe cannot leave a stale copy in submit.sh.files.
                recipe_arg = shlex.quote(str(build.recipe))
            else:
                payload=base64.b64encode(build.recipe.read_bytes()).decode()
                launch='original=$(mktemp)\ntrap \'rm -f -- "$original"\' EXIT\nprintf %s PAYLOAD | base64 -d > "$original"\nIFS= read -r first < "$original" || true\nif [[ "$first" == \'#!\'* ]]; then chmod u+x "$original"; "$original" "$@"; else bash "$original" "$@"; fi\n'.replace('PAYLOAD',shlex.quote(payload))
                write_script(wrapper, '#!/usr/bin/env bash\nset -euo pipefail\nOUTPUT_DEST=$SLURMY_BUILD_OUTPUT\nexport SLURMY_BUILD_OUTPUT=$SLURMY_BUILD_WORK\n'+launch+'\ncp -a "$SLURMY_BUILD_WORK/." "$OUTPUT_DEST/"\ntouch "$OUTPUT_DEST/.slurmy-built"\n')
                recipe_arg = f'"$HERE/recipe_{i}.sh"'
            command = ('"${SLURMY_PYTHON:-python3}" "$HERE/driver.py" --host "${SLURMY_HOST:-datalab}" '
                       f'--name "$WORKFLOW_NAME-build-{i}" --recipe {recipe_arg} '
                       + ''.join('--recipe-arg='+shlex.quote(arg)+' ' for arg in build.arguments)
                       + (f'--context {shlex.quote(str(build.source))} ' if build.source else '')
                       + ('--in-place ' if build.source == build.root else '')
                       + f'--output {shlex.quote(str(build.root))} '
                       + f'--build-policy {build.build_policy} --cpus-per-task=socket '
                       + f'--artifact={shlex.quote(build.artifact or ".slurmy-built")} '
                       + '--sbatch-option="--partition=${SLURMY_PARTITION:?Set SLURMY_PARTITION}" '
                       + '${SLURMY_RESERVATION:+--sbatch-option=--reservation=$SLURMY_RESERVATION} '
                       + '${SLURMY_ACCOUNT:+--sbatch-option=--account=$SLURMY_ACCOUNT}')
            commands.append((i, command))
        if parallel_builds and build_total > 1:
            build_lines += [f'echo "Starting {build_total} independent cluster builds in parallel"', 'build_pids=()', 'build_names=()']
            for i, command in commands:
                build_lines += [f'echo "Starting resource-{i}"', command + ' &', 'build_pids+=("$!")', f'build_names+=(resource-{i})']
            build_lines += ['failed=0', 'for index in "${!build_pids[@]}"; do',
                            '    if wait "${build_pids[$index]}"; then',
                            '        echo "Finished ${build_names[$index]}"',
                            '    else',
                            '        echo "Failed ${build_names[$index]}" >&2',
                            '        failed=1',
                            '    fi',
                            'done', '(( failed == 0 )) || exit 1']
        else:
            for number, (i, command) in enumerate(commands, 1):
                build_lines += [f'echo "Build progress: {number}/{build_total} resources (resource-{i})"', command]
        write_script(assets / "builds/run.sh", "\n".join(build_lines) + "\n")
        # NUL-separated paths allow spaces, commas, quotes and newlines.
        if progress:
            progress('phase', 'Collecting resource paths for packaging', 0, 0)
        entries = sorted(set(roots + [source for source in axioms if not str(source).startswith('tptp:')] + [Path(t['solver_root']) for t in tasks] +
                             [Path(t['problem']) for t in tasks if not t['remote_problem']]))
        entry_set = set(entries)
        selected = []
        for index, path in enumerate(entries, 1):
            if not any(parent in entry_set for parent in path.parents):
                selected.append(path)
            if progress and (index % 1000 == 0 or index == len(entries)):
                progress('paths', 'Collecting resource paths for packaging', index, len(entries))
        entries = selected
        if progress:
            progress('phase', 'Writing resource manifest', 0, 0)
        (assets / "archive-paths.txt").write_bytes(b"".join(str(p).lstrip("/").encode() + b"\0" for p in entries))
    except Exception:
        shutil.rmtree(assets)
        output.unlink(missing_ok=True)
        raise
    return output


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jobpairs", type=Path)
    parser.add_argument("building", type=Path)
    parser.add_argument("resource_limiter_template", type=Path)
    parser.add_argument('--axioms-file', type=Path, help='optional file with one TPTP axiom path or glob per line')
    parser.add_argument("--materialize-calls",action="store_true",help="also write individual pair scripts locally for inspection")
    parser.add_argument('--no-computer-info', action='store_true', help='omit computer information from solver output')
    parser.add_argument("--batch-size", type=int, default=1, help="maximum solver-problem pairs owned by one SLURM array task; solver-problem pairs run sequentially (default: 1)")
    parser.add_argument("--max-outstanding", type=int, default=50, help="maximum pending/running SLURM tasks from one job (default: 50)")
    parser.add_argument('--pair-order', choices=PAIR_ORDERS,
                        help='declared input ordering; all orderings pack batches by matching SLURM allocation settings, allowing mixed solvers')
    args = parser.parse_args(argv)
    def report(kind,label,completed,total):
        print(f'Preparation progress: {label}'+(f': {completed}/{total}' if total else ''),flush=True)
    try:
        output = generate(args.jobpairs.resolve(), args.building.resolve(), args.resource_limiter_template.resolve(),
                          args.axioms_file.resolve() if args.axioms_file else None, args.batch_size,
                          max_outstanding=args.max_outstanding, pair_order=args.pair_order,
                          materialize_calls=args.materialize_calls,progress=report,
                          get_computer_info=not args.no_computer_info)
    except (SlurmyError, OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Prepared {output}. Inspect its companion files, then run it to build and submit.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
