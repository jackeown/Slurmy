#!/usr/bin/env python
"""Build portable job artifacts on a SLURM compute node and download them."""

from __future__ import annotations
# Support direct invocation and copied build drivers.
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from ssh_transport import connection_options


import argparse
import hashlib
import json
import fcntl
import os
from pathlib import PurePosixPath
import re
import shlex
import shutil
import subprocess
import tarfile
import tempfile
import time
from typing import Sequence


VERSION = "0.1.0"
DEFAULT_HOST = os.environ.get("SLURMY_HOST", "datalab")
NAME_RE = re.compile(r"^[A-Za-z0-9._-]+$")
MEMORY_RE = re.compile(r"^([0-9]+)(MB|MiB|GB|GiB|TB|TiB|M|G|T)?$", re.IGNORECASE)
FINAL_STATES = {
    "BOOT_FAIL",
    "CANCELLED",
    "COMPLETED",
    "DEADLINE",
    "FAILED",
    "NODE_FAIL",
    "OUT_OF_MEMORY",
    "PREEMPTED",
    "REVOKED",
    "TIMEOUT",
}


class BuildError(RuntimeError):
    """A remote build could not be prepared or completed."""


def run(
    command: Sequence[str], *, input_text: str | None = None, capture: bool = False
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            input=input_text,
            text=True,
            check=True,
            capture_output=capture,
            timeout=3600 if command[0] in {'scp', 'rsync'} else 120,
        )
    except OSError as exc:
        raise BuildError(f"could not run {command[0]}: {exc}") from exc
    except subprocess.TimeoutExpired as exc:
        raise BuildError(f"{command[0]} timed out; check remote state before retrying") from exc
    except subprocess.CalledProcessError as exc:
        detail = (exc.stderr or exc.stdout or "").strip()
        suffix = f"\n{detail}" if detail else ""
        raise BuildError(f"command failed: {shlex.join(command)}{suffix}") from exc


def checked_artifact(value: str) -> str:
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(part in {"", ".", ".."} for part in path.parts):
        raise argparse.ArgumentTypeError("artifact paths must be relative and may not contain . or ..")
    return path.as_posix()


def checked_sbatch_option(value: str) -> str:
    if not value.startswith("--") or "\n" in value or "\r" in value:
        raise argparse.ArgumentTypeError("use a single-line --OPTION=VALUE")
    if value.startswith("--job-name") or value.startswith("--output") or value.startswith("--error"):
        raise argparse.ArgumentTypeError("Slurmy manages job-name, output, and error")
    return value


def checked_time(value: str) -> str:
    if not re.fullmatch(r"[0-9:-]+", value):
        raise argparse.ArgumentTypeError("time may contain only digits, colons, and hyphens")
    return value


def slurm_memory(value: str) -> str:
    """Convert explicit decimal/binary units to the MiB syntax SLURM accepts."""

    match = MEMORY_RE.fullmatch(value.strip())
    if not match or int(match.group(1)) <= 0:
        raise argparse.ArgumentTypeError(
            "memory must be a positive integer followed by M, G, T, MB, MiB, GB, GiB, TB, or TiB"
        )
    number = int(match.group(1))
    unit = (match.group(2) or "M").lower()
    bytes_per_unit = {
        "m": 1024**2,
        "mib": 1024**2,
        "mb": 1000**2,
        "g": 1024**3,
        "gib": 1024**3,
        "gb": 1000**3,
        "t": 1024**4,
        "tib": 1024**4,
        "tb": 1000**4,
    }[unit]
    mebibytes = (number * bytes_per_unit + 1024**2 - 1) // 1024**2
    return f"{mebibytes}M"


def make_context_archive(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise BuildError(f"build context is not a directory: {source}")
    with tarfile.open(destination, "w:gz") as archive:
        for child in sorted(source.iterdir(), key=lambda path: path.name):
            archive.add(child, arcname=child.name,
                        filter=lambda entry: None if PurePosixPath(entry.name).name in CACHE_FILES else entry)


def build_job_script(args: argparse.Namespace, *, has_context: bool) -> str:
    directives = [
        f"#SBATCH --job-name={args.name}",
        "#SBATCH --output=build.log",
        "#SBATCH --error=build.log",
        f"#SBATCH --cpus-per-task={args.cpus_per_task}",
        "#SBATCH --nodes=1",
        "#SBATCH --ntasks=1",
        f"#SBATCH --mem={args.memory}",
        f"#SBATCH --time={args.time}",
        *(f"#SBATCH {option}" for option in args.sbatch_option),
    ]
    context_setup = (
        'tar -xzf "$SLURMY_BUILD_ROOT/context.tar.gz" -C "$SLURMY_BUILD_WORK"'
        if has_context
        else ": # No local build context was supplied."
    )
    artifact_checks = "\n".join(
        f'[[ -e "$SLURMY_BUILD_OUTPUT"/{shlex.quote(artifact)} ]] || '
        f'{{ printf "%s\\n" {shlex.quote(f"Missing required artifact: {artifact}")} >&2; exit 1; }}'
        for artifact in args.artifact
    )
    return f"""#!/usr/bin/env bash
{chr(10).join(directives)}

# Generated by slurmy-build.py. This runs on one SLURM compute node.
set -euo pipefail

SLURMY_BUILD_ROOT=${{SLURM_SUBMIT_DIR:?SLURM did not set SLURM_SUBMIT_DIR}}
export SLURMY_BUILD_ROOT
export SLURMY_BUILD_WORK="$SLURMY_BUILD_ROOT/work"
export SLURMY_BUILD_OUTPUT="$SLURMY_BUILD_ROOT/output"

rm -rf -- "$SLURMY_BUILD_WORK" "$SLURMY_BUILD_OUTPUT"
mkdir -p -- "$SLURMY_BUILD_WORK" "$SLURMY_BUILD_OUTPUT"
{context_setup}

cd -- "$SLURMY_BUILD_WORK"
bash "$SLURMY_BUILD_ROOT/recipe.sh"

{'# The recipe built in its working folder; return the full resulting tree.' if args.in_place else '# The recipe wrote selected runtime files to SLURMY_BUILD_OUTPUT.'}
{'cp -a -- "$SLURMY_BUILD_WORK/." "$SLURMY_BUILD_OUTPUT/"' if args.in_place else ':'}

{artifact_checks}
echo "Slurmy build completed successfully."
"""


REMOTE_PREPARE = r"""
set -euo pipefail
umask 077
build_id=$1
install -d -m 700 "$HOME/slurmy" "$HOME/slurmy/builds"
root="$HOME/slurmy/builds/$build_id"
staging="$HOME/slurmy/builds/.staging-$build_id"
[[ ! -e "$root" && ! -e "$staging" ]] || { echo "Build directory already exists: $root" >&2; exit 1; }
mkdir -m 700 -p -- "$staging"
"""


REMOTE_SUBMIT = r"""
set -euo pipefail
build_id=$1
root="$HOME/slurmy/builds/$build_id"
staging="$HOME/slurmy/builds/.staging-$build_id"
[[ -f "$staging/job.sh" && -f "$staging/recipe.sh" && ! -e "$root" ]] || {
    echo 'Build upload is incomplete or already published.' >&2; exit 1;
}
mv -T -- "$staging" "$root"
cd -- "$root"
sbatch --parsable job.sh
"""


REMOTE_STATE = r"""
set -euo pipefail
job_id=$1
state=$(squeue --noheader --jobs "$job_id" --format='%T' | head -n 1)
if [[ -n "$state" ]]; then
    printf '%s\n' "$state"
    exit 0
fi
sacct -n -X -j "$job_id" --format=JobIDRaw,State --parsable2 2>/dev/null |
    awk -F'|' -v id="$job_id" '$1 == id { print $2; exit }'
"""


REMOTE_LOG = r"""
set -euo pipefail
build_id=$1
log="$HOME/slurmy/builds/$build_id/build.log"
if [[ -f "$log" ]]; then cat -- "$log"; fi
"""


def normalize_state(value: str) -> str:
    return value.strip().upper().split()[0].rstrip("+") if value.strip() else ""


def execute_build(args: argparse.Namespace) -> None:
    recipe = Path(args.recipe).expanduser().resolve()
    output = Path(args.output).expanduser().resolve()
    if not recipe.is_file():
        raise BuildError(f"build recipe is not a regular file: {recipe}")
    if not args.artifact:
        raise BuildError("at least one --artifact is required")

    build_id = f"{args.name}_{int(time.time())}_{os.getpid()}"
    remote_dir = f"$HOME/slurmy/builds/{build_id}"
    print(f"Slurmy build: {build_id}", flush=True)
    print(f"Remote directory: {args.host}:{remote_dir}", flush=True)

    with tempfile.TemporaryDirectory(prefix="slurmy-build-") as temporary:
        staging = Path(temporary)
        shutil.copy2(recipe, staging / "recipe.sh")
        has_context = args.context is not None
        if has_context:
            make_context_archive(Path(args.context).expanduser().resolve(), staging / "context.tar.gz")
        job_script = staging / "job.sh"
        job_script.write_text(build_job_script(args, has_context=has_context), encoding="utf-8")
        job_script.chmod(0o755)

        run(["ssh", *connection_options(), "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", args.host, "bash", "-s", "--", build_id], input_text=REMOTE_PREPARE)
        run(["scp", *connection_options(), "-q", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", *(str(path) for path in staging.iterdir()), f"{args.host}:slurmy/builds/.staging-{build_id}/"])
        submitted = run(
            ["ssh", *connection_options(), "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", args.host, "bash", "-s", "--", build_id],
            input_text=REMOTE_SUBMIT,
            capture=True,
        )

    job_id = submitted.stdout.strip().split(";", 1)[0]
    if not job_id.isdigit():
        raise BuildError(f"sbatch returned an invalid job ID: {submitted.stdout.strip()!r}")
    print(f"SLURM job: {job_id}", flush=True)

    previous = ""
    missed_polls = 0
    while True:
        try:
            state_result = run(
                ["ssh", *connection_options(), "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", args.host, "bash", "-s", "--", job_id],
                input_text=REMOTE_STATE,
                capture=True,
            )
        except BuildError:
            missed_polls += 1
            if missed_polls >= 10:
                raise
            print(f"Build status temporarily unreachable; retrying ({missed_polls}/10).", flush=True)
            time.sleep(args.poll_interval)
            continue
        missed_polls = 0
        state = normalize_state(state_result.stdout)
        if state and state != previous:
            print(f"State: {state}", flush=True)
            previous = state
        if state in FINAL_STATES:
            break
        time.sleep(args.poll_interval)

    if state != "COMPLETED":
        log = run(
            ["ssh", *connection_options(), "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", "--", args.host, "bash", "-s", "--", build_id],
            input_text=REMOTE_LOG,
            capture=True,
        ).stdout.rstrip()
        if log:
            print("\n--- remote build log ---", file=sys.stderr)
            print(log, file=sys.stderr)
        raise BuildError(f"SLURM build job {job_id} ended in state {state or 'UNKNOWN'}")

    output.mkdir(parents=True, exist_ok=True)
    try:
        args._inputs_unchanged = input_fingerprint(args) == getattr(args, '_initial_fingerprint', None)
    except (BuildError, OSError):
        args._inputs_unchanged = False
    run(["rsync", "-a", "--delay-updates", "--partial-dir=.rsync-partial", "-e", shlex.join(["ssh", *connection_options(), "-o", "BatchMode=yes", "-o", "ConnectTimeout=10"]), "--", f"{args.host}:slurmy/builds/{build_id}/output/", f"{output}/"])
    for artifact in args.artifact:
        if not (output / artifact).exists():
            raise BuildError(f"downloaded output is missing required artifact: {artifact}")
    print(f"Downloaded artifacts to {output}")


CACHE_FILES = {'.slurmy-build-cache.json', '.slurmy-build-cache.lock'}


def tree_fingerprint(root: Path) -> str:
    """Hash names, file contents, permissions and symlink targets without following directories."""
    digest = hashlib.sha256()
    for path in sorted(root.rglob('*')):
        if path.name in CACHE_FILES:
            continue
        relative = path.relative_to(root).as_posix()
        stat = path.lstat()
        digest.update(json.dumps([relative, stat.st_mode]).encode())
        if path.is_symlink():
            # Symlinked inputs may live outside the source directory. Disable
            # reuse rather than claim we tracked their external contents.
            raise BuildError('Source/output contains symlinks; build reuse is disabled for this resource.')
        elif path.is_file():
            with path.open('rb') as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                    digest.update(chunk)
        elif not path.is_dir():
            raise BuildError(f'Cannot fingerprint special file: {path}')
    return digest.hexdigest()


def input_fingerprint(args) -> str:
    settings = {key: value for key, value in vars(args).items()
                if key not in {'name', 'poll_interval', 'build_policy'} and not key.startswith('_')}
    settings['recipe_contents'] = Path(args.recipe).expanduser().read_text()
    settings['source_contents'] = tree_fingerprint(Path(args.context).expanduser()) if args.context else None
    settings['driver_contents'] = Path(__file__).read_text()
    return hashlib.sha256(json.dumps(settings, sort_keys=True).encode()).hexdigest()


def cached_build(args) -> None:
    output = Path(args.output).expanduser().resolve()
    output.mkdir(parents=True, exist_ok=True)
    cache = output / '.slurmy-build-cache.json'
    # Serialize builds sharing one runtime root, including builds from other workflows.
    with (output / '.slurmy-build-cache.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        before = None
        try:
            before = input_fingerprint(args)
            recorded = json.loads(cache.read_text()) if cache.exists() else {}
            valid = all((output / artifact).is_file() for artifact in args.artifact or [])
            if (args.build_policy == 'changed' and valid and recorded.get('inputs') == before
                    and recorded.get('outputs') == tree_fingerprint(output)):
                print(f'Reusing successful build in {output}: source, recipe, settings and outputs unchanged.', flush=True)
                return
        except (BuildError, OSError, ValueError) as exc:
            print(f'Build cache unavailable: {exc}', flush=True)
        cache.unlink(missing_ok=True)
        args._initial_fingerprint = before
        execute_build(args)
        # Downloading an in-place build changes the source tree itself. Record
        # that final tree, but only if local inputs stayed unchanged during the build.
        if before is not None and getattr(args, '_inputs_unchanged', False):
            try:
                record = dict(inputs=input_fingerprint(args), outputs=tree_fingerprint(output))
                temporary = cache.with_suffix('.tmp')
                temporary.write_text(json.dumps(record))
                temporary.replace(cache)
            except (BuildError, OSError, ValueError) as exc:
                print(f'Build succeeded; reuse could not be recorded: {exc}', flush=True)
        else:
            print('Build succeeded; inputs changed during the build or cannot be tracked. Next job will rebuild.', flush=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="slurmy-build.py",
        description="Run a build recipe as a SLURM job and download its artifacts.",
    )
    parser.add_argument("--version", action="version", version=f"Slurmy build {VERSION}")
    parser.add_argument("--host", default=DEFAULT_HOST, help="SSH host alias (default: $SLURMY_HOST, or datalab)")
    parser.add_argument("--name", required=True, help="short build name used in job and directory names")
    parser.add_argument("--recipe", required=True, metavar="FILE", help="Bash build recipe to run on the compute node")
    parser.add_argument("--context", metavar="DIRECTORY", help="optional local directory unpacked into the build work directory")
    parser.add_argument("--output", required=True, metavar="DIRECTORY", help="local directory that receives the built artifacts")
    parser.add_argument("--artifact", action="append", type=checked_artifact, metavar="RELATIVE_PATH", help="required path below the output directory; repeat as needed")
    parser.add_argument("--build-policy", choices=("always", "changed"), default="always")
    parser.add_argument("--in-place", action="store_true", help="run the recipe in the copied source directory and download its entire resulting tree; no Slurmy variables are needed")
    parser.add_argument("--cpus-per-task", type=int, default=4)
    parser.add_argument("--memory", type=slurm_memory, default="4096M", help="SLURM memory request (default: 4GiB)")
    parser.add_argument("--time", type=checked_time, default="00:30:00", help="SLURM time request (default: 00:30:00)")
    parser.add_argument("--sbatch-option", action="append", type=checked_sbatch_option, default=[], metavar="--OPTION=VALUE")
    parser.add_argument("--poll-interval", type=float, default=2.0, metavar="SECONDS")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not NAME_RE.fullmatch(args.name):
        raise SystemExit("--name may contain only letters, digits, ., _, and -")
    if args.cpus_per_task <= 0:
        raise SystemExit("--cpus-per-task must be greater than zero")
    if args.poll_interval < 0.2:
        raise SystemExit("--poll-interval must be at least 0.2 seconds")
    try:
        cached_build(args)
    except (BuildError, OSError, tarfile.TarError) as exc:
        print(f"Slurmy build: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
