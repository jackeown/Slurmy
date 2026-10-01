#!/usr/bin/env python
"""Prepare an explicit table of jobpairs for Slurm; Python runs locally only."""
from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import glob
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import random
import re
import shlex
import shutil
import sys
from typing import Sequence

VERSION = "1.0.0"
COLUMNS = ("command", "solver_directory", "problem", "wc_limit", "cpu_limit", "mem_limit", "cores",
           "cpus", "exclusive_cpu", "exclusive_node")
RESULT_COLUMNS = ("task_id", "complete", "status", "return_code", "wall_seconds",
                  "cpu_seconds", "user_seconds", "system_seconds", "cpu_usage_percent",
                  "max_virtual_memory_kib", "max_memory_kib", "timed_out", "memory_out",
                  "task_key", "archive", "szs_status")
REPO = Path(__file__).resolve().parent
PLACEHOLDER = re.compile(r"\{\{([a-z_]+)\}\}")
PAIR_ORDERS = ("problem-major", "solver-major", "random")


class SlurmyError(ValueError):
    pass


def order_jobpairs(rows: list[dict], order: str) -> list[dict]:
    """Order calls reproducibly without changing their contents."""
    if order not in PAIR_ORDERS:
        raise SlurmyError(f"jobpair order must be one of: {', '.join(PAIR_ORDERS)}")
    result = list(rows)
    if order == "problem-major":
        result.sort(key=lambda row: row["problem"])
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


def positive(value: str, name: str) -> int:
    if not re.fullmatch(r"[1-9][0-9]*", str(value)):
        raise SlurmyError(f"{name}: expected a positive integer, got {value!r}")
    return int(value)


def boolean(value: str, name: str) -> bool:
    if value.strip().lower() not in {"", "true", "false"}:
        raise SlurmyError(f"{name}: expected true or false")
    return value.strip().lower() == "true"


def memory_bytes(value: str) -> int:
    match = re.fullmatch(r"([1-9][0-9]*)\s*(MB|MiB|GB|GiB|TB|TiB)?", value, re.I)
    if not match:
        raise SlurmyError(f"invalid mem_limit {value!r}; use e.g. 1200MiB or 2GB")
    factors = {"mb": 10**6, "mib": 2**20, "gb": 10**9, "gib": 2**30,
               "tb": 10**12, "tib": 2**40}
    return int(match[1]) * factors[(match[2] or "MB").lower()]


def read_pairs(filename: Path) -> list[dict]:
    tasks = []
    with filename.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        headers = reader.fieldnames or []
        required = set(COLUMNS) - {"exclusive_cpu", "exclusive_node"}
        if len(headers) != len(set(headers)) or not required <= set(headers) or set(headers) - set(COLUMNS):
            raise SlurmyError(f"{filename}: columns must be {', '.join(COLUMNS)}; exclusivity columns may be omitted")
        for row in reader:
            label = f"{filename}: row ending at line {reader.line_num}"
            if None in row or any(row.get(key) is None for key in required):
                raise SlurmyError(f"{label}: wrong number of CSV fields")
            command = row["command"].strip()
            if not command or "\x00" in command:
                raise SlurmyError(f"{label}: command is empty or contains NUL")
            problem = path_at(row["problem"], filename.parent)
            if not row["problem"] or not problem.is_file():
                raise SlurmyError(f"{label}: problem does not exist: {problem}")
            solver_root = path_at(row["solver_directory"], filename.parent)
            if not row["solver_directory"] or not solver_root.is_dir():
                raise SlurmyError(f"{label}: solver_directory is not an existing directory: {solver_root}")
            task = {"task_id": len(tasks), "command": command, "problem": str(problem),
                    "solver_root": str(solver_root),
                    "system": Path(shlex.split(command)[0]).name}
            for key in ("wc_limit", "cpu_limit", "cores", "cpus"):
                task[key] = positive(row[key].strip(), f"{label}: {key}")
            task["mem_limit"] = memory_bytes(row["mem_limit"].strip())
            for key in ("exclusive_cpu", "exclusive_node"):
                task[key] = boolean(row.get(key) or "", f"{label}: {key}")
            task["task_key"] = hashlib.sha256(json.dumps(task, sort_keys=True).encode()).hexdigest()[:16]
            tasks.append(task)
    if not tasks:
        raise SlurmyError("jobpairs.csv contains no calls")
    return tasks


@dataclass(frozen=True)
class BuildSpec:
    root: Path
    recipe: Path | None
    source: Path | None = None
    artifact: str | None = None


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
            builds.append(BuildSpec(root, recipe, source, artifact))
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


def read_axioms(filename: Path | None) -> tuple[list[Path], dict[Path, Path]]:
    """Expand globs and map files into one job-local Axioms/ directory."""
    if filename is None:
        return [], {}
    files = set()
    for line in filename.read_text(encoding='utf-8').splitlines():
        if not line.strip():
            continue
        pattern = str(path_at(line.strip(), filename.parent))
        matches = {Path(name).resolve() for name in glob.glob(pattern, recursive=True) if Path(name).is_file()}
        if not matches:
            raise SlurmyError(f"axiom glob matched no files: {line}")
        files.update(matches)
    if not files:
        raise SlurmyError('axiom-globs.txt contains no axiom files')
    destinations = {}
    for file in files:
        parts = file.parts
        relative = Path(*parts[parts.index('Axioms') + 1:]) if 'Axioms' in parts else Path(file.name)
        if relative in destinations.values():
            raise SlurmyError(f'custom axioms would overwrite the same Axioms/{relative}; choose one source')
        destinations[file] = relative
    return sorted(files), destinations


def remote_path(path: Path) -> str:
    return '"${JOB_DIR}/rootfs"/' + shlex.quote(str(path).lstrip("/"))


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


def generate(jobpairs: Path, building: Path, limiter_file: Path, degree: int,
             axioms_file: Path | None = None, batch_size: int | None = None) -> Path:
    tasks = read_pairs(jobpairs)
    batch_size = batch_size or degree
    positive(str(batch_size), 'batch size')
    builds = read_builds(building)
    axioms, axiom_destinations = read_axioms(axioms_file)
    limiter = limiter_file.read_text(encoding="utf-8").strip()
    if not limiter or "{{solver_command}}" not in limiter:
        raise SlurmyError("resource_limiter_template.txt must contain {{solver_command}}")
    # First word is an executable path, not an arbitrary shell expression.
    words = shlex.split(limiter)
    limiter_path = path_at(words[0], limiter_file.parent)
    roots = [build.root for build in builds]
    if not any(limiter_path.is_relative_to(root) for root in roots):
        raise SlurmyError("limiter executable must be inside a building.txt resource root")
    if not limiter_path.is_file() and not any(build.recipe and limiter_path.is_relative_to(build.root) for build in builds):
        raise SlurmyError(f"limiter does not exist and no recipe builds it: {limiter_path}")
    limiter = re.sub(r"^\s*(?:'[^']*'|\"[^\"]*\"|\S+)", lambda _: remote_path(limiter_path), limiter, count=1)
    # Every command runs in its explicitly selected, mirrored solver directory.
    problems = {Path(t["problem"]) for t in tasks}
    packaged_roots = set(roots + [Path(t['solver_root']) for t in tasks])
    for task in tasks:
        lexer = shlex.shlex(task['command'], posix=True, punctuation_chars=True)
        lexer.whitespace_split = True
        command_paths = [Path(word) for word in lexer
                         if word.startswith('/') and (Path(word) in problems or
                             any(Path(word).is_relative_to(root) for root in packaged_roots))]
        values = {key: str(task[key]) for key in ("wc_limit", "cpu_limit", "cores", "cpus")}
        values.update(mem_limit=str(math.ceil(task["mem_limit"] / 10**6)),
                      mem_limit_mib=str(math.ceil(task["mem_limit"] / 2**20)),
                      problem=remote_path(Path(task["problem"])))
        task["remote_command"] = render(relocate(task["command"], command_paths), values)
        values["solver_command"] = '/bin/bash "$CALL_DIR/solver.sh"'
        for name in ("watcher_log", "var_file", "solver_log"):
            values[name] = '"${' + name.upper() + '}"'
        task["limiter_command"] = render(limiter, values)
    batches: list[list[dict]] = []
    # Node-exclusive calls get their own batch. Other batches may contain
    # several sequential waves of at most degree concurrent calls.
    for task in tasks:
        if task["exclusive_node"]:
            batches.append([task])
        elif (not batches or len(batches[-1]) >= batch_size or batches[-1][0]["exclusive_node"]
              or batches[-1][0]["exclusive_cpu"] != task["exclusive_cpu"]):
            batches.append([task])
        else:
            batches[-1].append(task)
        task["batch_id"] = len(batches) - 1
    output = jobpairs.parent / "submit.sh"
    assets = jobpairs.parent / "submit.sh.files"
    if output.exists() or assets.exists():
        raise SlurmyError("submit.sh or submit.sh.files already exists; run make clean before regenerating")
    assets.mkdir()
    try:
        (assets / "calls").mkdir()
        (assets / "plans").mkdir()
        (assets / "builds").mkdir()
        metadata = {"slurmy_version": VERSION, "workflow_directory": str(jobpairs.parent), "task_count": len(tasks), "batch_count": len(batches),
                    "batch_size": batch_size, "deg_par": degree, "result_columns": RESULT_COLUMNS,
                    "planned_batch_tasks": [[t["task_id"] for t in batch] for batch in batches],
                    "batch_policy": "sequential waves up to deg_par; node-exclusive calls get a batch of their own",
                    "buffers": {"per_call_seconds": 30, "per_call_memory_mib": 128, "batch_seconds": 60}}
        (assets / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
        (assets / "manifest.jsonl").write_text("".join(json.dumps(t) + "\n" for t in tasks))
        for task in tasks:
            call = assets / "calls" / str(task["task_id"])
            call.mkdir()
            write_script(call / "solver.sh", "#!/usr/bin/env bash\n" +
                         "# Capture the prover's stderr separately from its stdout.\n" +
                         "( " + task["remote_command"] + " ) 2> \"$SOLVER_STDERR_LOG\"\n")
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
            write_script(call / "limiter.sh", limiter_script)
            config = "".join(key.upper() + "=" + shlex.quote(str(value)) + "\n" for key, value in {
                "task_id": task["task_id"], "task_key": task["task_key"], "batch_id": task["batch_id"],
                "cores": task["cores"], "cpus": task["cpus"], "wc_limit": task["wc_limit"],
                    "solver_root_rel": task['solver_root'].lstrip("/"),
                    "tptp_root_rel": '.slurmy-tptp' if axioms else '',
                    "tptp_default_root": os.environ.get('SLURMY_TPTP_ROOT', '/share/Slurmy-TPTP-v9.2.1')}.items())
            (call / "config.sh").write_text(config)
        for i, batch in enumerate(batches):
            config = shell_array("TASK_IDS", [t["task_id"] for t in batch])
            for name, key in (("TASK_CORES", "cores"), ("TASK_CPUS", "cpus"), ("TASK_WALL", "wc_limit")):
                config += shell_array(name, [t[key] for t in batch])
            config += shell_array("TASK_MEMORY", [math.ceil(t['mem_limit'] / 2**20) for t in batch])
            config += f"BATCH_ID={i}\nDEG_PAR={degree}\nEXCLUSIVE_CPU={int(batch[0]['exclusive_cpu'])}\nEXCLUSIVE_NODE={int(batch[0]['exclusive_node'])}\n"
            config += f"MEMORY_MIB={len(batch) * (max(math.ceil(t['mem_limit'] / 2**20) for t in batch) + 128)}\n"
            config += f"WALL_SECONDS={60 + sum(t['wc_limit'] + 30 for t in batch)}\n"
            config += f"MAX_CORES={max(t['cores'] for t in batch)}\nMAX_CPUS={max(t['cpus'] for t in batch)}\n"
            (assets / "plans" / f"batch_{i:06d}.sh").write_text(config)
        for name in ("submit.sh", "remote_prepare.sh", "remote_submit.sh", "batch.sh", "call.sh", "timed_call.sh", "csv.sh", "workflow_name.sh"):
            source = REPO / "templates" / name
            destination = output if name == "submit.sh" else assets / name
            # A generated file should look newly generated. copy2() preserved the
            # template's old mtime, which made make immediately call it stale.
            shutil.copyfile(source, destination)
            shutil.copymode(source, destination)
        # Reuse the local build driver. Remote recipes build in a disposable
        # copy of the declared root; downloaded files preserve that layout.
        shutil.copy2(REPO / "building-dependencies/slurmy-build.py", assets / "builds/driver.py")
        if axioms:
            axiom_lines = ['#!/usr/bin/env bash', 'set -euo pipefail',
                           'JOB_DIR=${JOB_DIR:?}', 'TPTP_DIR="$JOB_DIR/rootfs/.slurmy-tptp"',
                           'mkdir -p -- "$TPTP_DIR/Axioms"']
            for source, relative in axiom_destinations.items():
                target = 'Axioms/' + relative.as_posix()
                axiom_lines.append(f'mkdir -p -- "$TPTP_DIR"/{shlex.quote(str(Path(target).parent))}')
                axiom_lines.append(f'cp -- "$JOB_DIR/rootfs"/{shlex.quote(str(source).lstrip("/"))} '
                                   f'"$TPTP_DIR"/{shlex.quote(target)}')
            write_script(assets / 'prepare_axioms.sh', '\n'.join(axiom_lines) + '\n')
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
                body = build.recipe.read_text(encoding="utf-8")
                write_script(wrapper, '#!/usr/bin/env bash\nset -euo pipefail\nOUTPUT_DEST=$SLURMY_BUILD_OUTPUT\nexport SLURMY_BUILD_OUTPUT=$SLURMY_BUILD_WORK\n(\n' + body + '\n)\ncp -a "$SLURMY_BUILD_WORK/." "$OUTPUT_DEST/"\ntouch "$OUTPUT_DEST/.slurmy-built"\n')
                recipe_arg = f'"$HERE/recipe_{i}.sh"'
            command = ('python "$HERE/driver.py" --host "${SLURMY_HOST:-datalab}" '
                       f'--name "$WORKFLOW_NAME-build-{i}" --recipe {recipe_arg} '
                       + (f'--context {shlex.quote(str(build.source))} ' if build.source else '')
                       + f'--output {shlex.quote(str(build.root))} '
                       + f'--artifact={shlex.quote(build.artifact or ".slurmy-built")} '
                       + '--sbatch-option="--partition=${SLURMY_PARTITION:?Set SLURMY_PARTITION}"')
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
        entries = sorted(set([*roots, *axioms, *(Path(t['solver_root']) for t in tasks), *(Path(t["problem"]) for t in tasks)]))
        entries = [p for p in entries if not any(p != other and p.is_relative_to(other) for other in entries)]
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
    parser.add_argument("--deg_par", required=True, type=int, help="maximum concurrent jobpairs within one batch")
    parser.add_argument("--batch-size", type=int, help="maximum jobpairs owned by one Slurm array task (default: degree of parallelism)")
    args = parser.parse_args(argv)
    try:
        positive(str(args.deg_par), "--deg_par")
        output = generate(args.jobpairs.resolve(), args.building.resolve(), args.resource_limiter_template.resolve(),
                          args.deg_par, args.axioms_file.resolve() if args.axioms_file else None, args.batch_size)
    except (SlurmyError, OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Prepared {output}. Inspect its companion files, then run it to build and submit.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
