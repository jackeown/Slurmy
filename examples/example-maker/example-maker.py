#!/usr/bin/env python
"""Interactive Textual application for creating Slurmy example workflows."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
from typing import Sequence

try:
    from textual.app import App, ComposeResult
    from textual.containers import Horizontal, VerticalScroll
    from textual.widgets import Button, Footer, Header, Input, Label, Static, TabbedContent, TabPane, TextArea
except ModuleNotFoundError as exc:
    if exc.name == "textual" or (exc.name and exc.name.startswith("rich")):
        requirements = Path(__file__).resolve().parents[2] / "requirements.txt"
        print(
            "The Slurmy example maker needs Textual. Install it with:\n"
            f"  python -m pip install -r {requirements}",
            file=sys.stderr,
        )
        raise SystemExit(2) from None
    raise


DEFAULT_HOST = os.environ.get("SLURMY_HOST", "datalab")
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
SIMPLE_RE = re.compile(r"^[A-Za-z0-9._-]+$")
ARTIFACT_RE = re.compile(r"^[A-Za-z0-9._+/-]+$")
MEMORY_RE = re.compile(r"^[1-9][0-9]*(?:MB|MiB|GB|GiB|TB|TiB|M|G|T)?$", re.IGNORECASE)
TIME_RE = re.compile(r"^[0-9:-]+$")
PROBLEM_PLACEHOLDER_RE = re.compile(r"\{\{problem(?:=[^{}]+)?\}\}")
CPU_REQUESTS = {"1-core", "4-core", "8-core", "16-core", "32-core", "64-core"}


class WorkflowError(ValueError):
    """The form contains an invalid or incomplete workflow."""


@dataclass(frozen=True)
class Workflow:
    name: str
    host: str
    partition: str
    artifact: str
    solver_command: str
    problem_globs: tuple[str, ...]
    build_body: str
    cpu_limit: int
    wall_limit: int
    memory_limit: str
    cpu_request: str
    memory_request: str
    batch_size: int
    max_parallel: int
    worker_overhead: int
    build_cpus: int
    build_memory: str
    build_time: str

    def validate(self) -> None:
        if not SLUG_RE.fullmatch(self.name):
            raise WorkflowError("Workflow name must use lowercase letters, numbers, and hyphens.")
        if not SIMPLE_RE.fullmatch(self.host):
            raise WorkflowError("SSH host must be a simple host name or SSH alias.")
        if not SIMPLE_RE.fullmatch(self.partition):
            raise WorkflowError("Partition must contain only letters, numbers, ., _, or -.")
        artifact = PurePosixPath(self.artifact)
        if (
            not ARTIFACT_RE.fullmatch(self.artifact)
            or artifact.is_absolute()
            or any(part in {"", ".", ".."} for part in artifact.parts)
        ):
            raise WorkflowError("Artifact must be a safe path relative to the build output directory.")
        if "\n" in self.solver_command or "\r" in self.solver_command:
            raise WorkflowError("The solver invocation must be one command line.")
        if not PROBLEM_PLACEHOLDER_RE.search(self.solver_command):
            raise WorkflowError("The solver invocation must contain {{problem}} or {{problem=...}}.")
        if not self.problem_globs:
            raise WorkflowError("Enter at least one problem glob.")
        for pattern in self.problem_globs:
            if any(character.isspace() for character in pattern) or "#" in pattern:
                raise WorkflowError("Problem globs may not contain whitespace or # characters.")
        if not self.build_body.strip():
            raise WorkflowError("Enter the commands that build and install the solver artifact.")
        if "OWNER/PROJECT" in self.build_body:
            raise WorkflowError("Replace OWNER/PROJECT in the example build commands.")
        for label, value in (
            ("CPU limit", self.cpu_limit),
            ("wall limit", self.wall_limit),
            ("batch size", self.batch_size),
            ("maximum parallel tasks", self.max_parallel),
            ("build CPUs", self.build_cpus),
        ):
            if value <= 0:
                raise WorkflowError(f"{label} must be greater than zero.")
        if self.worker_overhead < 0:
            raise WorkflowError("Worker overhead cannot be negative.")
        if self.cpu_request not in CPU_REQUESTS:
            raise WorkflowError("CPU request must be one of: " + ", ".join(sorted(CPU_REQUESTS)))
        for label, value in (
            ("solver memory limit", self.memory_limit),
            ("Slurm memory request", self.memory_request),
            ("build memory", self.build_memory),
        ):
            if not MEMORY_RE.fullmatch(value):
                raise WorkflowError(f"{label} needs a positive value such as 2GiB or 2300MiB.")
        if not TIME_RE.fullmatch(self.build_time):
            raise WorkflowError("Build time may contain only digits, colons, and hyphens.")


def positive_integer(value: str, label: str, *, zero_allowed: bool = False) -> int:
    try:
        number = int(value)
    except ValueError as exc:
        raise WorkflowError(f"{label} must be an integer.") from exc
    if number < 0 or (number == 0 and not zero_allowed):
        qualifier = "zero or greater" if zero_allowed else "greater than zero"
        raise WorkflowError(f"{label} must be {qualifier}.")
    return number


def make_problem_globs(patterns: tuple[str, ...]) -> str:
    rendered = []
    for pattern in patterns:
        if pattern.startswith("/") or pattern.startswith("$("):
            rendered.append(pattern)
        else:
            rendered.append(f"$(CURDIR)/{pattern.removeprefix('./')}")
    if len(rendered) == 1:
        return f"PROBLEM_GLOBS := {rendered[0]}"
    continuation = " \\\n  ".join(rendered)
    return f"PROBLEM_GLOBS := \\\n  {continuation}"


def workflow_makefile(workflow: Workflow) -> str:
    return "\n".join(
        [
            "# Generated by example-maker.py. Edit any value and run make again.",
            f"SLURMY_HOST := {workflow.host}",
            f"SLURMY_BUILD_PARTITION := {workflow.partition}",
            f"EXAMPLE_NAME := {workflow.name}",
            f"SOLVER_ARTIFACT := {workflow.artifact}",
            make_problem_globs(workflow.problem_globs),
            f"CPU_LIMIT := {workflow.cpu_limit}",
            f"WALL_LIMIT := {workflow.wall_limit}",
            f"MEMORY_LIMIT := {workflow.memory_limit}",
            f"CPU_REQUEST := {workflow.cpu_request}",
            f"MEMORY_REQUEST := {workflow.memory_request}",
            f"BATCH_SIZE := {workflow.batch_size}",
            f"MAX_PARALLEL := {workflow.max_parallel}",
            f"WORKER_OVERHEAD := {workflow.worker_overhead}",
            f"BUILD_CPUS := {workflow.build_cpus}",
            f"BUILD_MEMORY := {workflow.build_memory}",
            f"BUILD_TIME := {workflow.build_time}",
            "",
        ]
    )


def build_recipe(workflow: Workflow) -> str:
    return f"""#!/usr/bin/env bash
# Generated by example-maker.py. This runs in a Slurm job on a compute node.
set -euo pipefail

: "${{SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}}"
: "${{SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}}"

{workflow.build_body.strip()}
"""


def generated_readme(workflow: Workflow) -> str:
    return f"""# {workflow.name} Slurmy workflow

This workflow was generated by `examples/example-maker/example-maker.py` and
uses the shared `example-template` Makefile.

Add benchmark files below `problems/`, review `workflow.mk`, `solver.solver`,
and `build.sh`, then run:

```bash
make
make submit
make monitor   # Or use make sync to download results.
```

The solver and runsolver are built in Slurm jobs on `{workflow.host}` before
the experiment is packaged.
"""


def generate_workflow(workflow: Workflow, output_root: Path) -> Path:
    workflow.validate()
    output_root = output_root.resolve()
    repository = Path(__file__).resolve().parents[2]
    template = repository / "examples" / "example-template"
    destination = output_root / f"example-{workflow.name}"
    if destination.exists():
        raise WorkflowError(f"Destination already exists: {destination}")
    if not (template / "Makefile").is_file():
        raise WorkflowError(f"Template Makefile is missing: {template / 'Makefile'}")

    staging = output_root / f".example-{workflow.name}.creating-{os.getpid()}"
    if staging.exists():
        raise WorkflowError(f"Temporary destination already exists: {staging}")
    try:
        staging.mkdir(parents=True)
        makefile = (template / "Makefile").read_text(encoding="utf-8")
        repository_relative = os.path.relpath(repository, destination)
        makefile = makefile.replace(
            "REPO_ROOT := ../..", f"REPO_ROOT := {repository_relative}", 1
        )
        (staging / "Makefile").write_text(makefile, encoding="utf-8")
        (staging / "workflow.mk").write_text(workflow_makefile(workflow), encoding="utf-8")
        (staging / "solver.solver").write_text(
            f"./bin\n{workflow.solver_command.strip()}\n", encoding="utf-8"
        )
        recipe = staging / "build.sh"
        recipe.write_text(build_recipe(workflow), encoding="utf-8")
        recipe.chmod(0o755)
        (staging / "README.md").write_text(generated_readme(workflow), encoding="utf-8")
        (staging / "problems").mkdir()
        (staging / "problems" / ".gitkeep").touch()
        staging.rename(destination)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return destination


class ExampleMakerApp(App[None]):
    TITLE = "Slurmy Example Maker"
    SUB_TITLE = "Create a reproducible theorem-prover workflow"
    BINDINGS = [("ctrl+g", "generate", "Generate"), ("ctrl+q", "quit", "Quit")]

    CSS = """
    Screen { background: #09101f; color: #dce7f7; }
    Header { background: #101b33; }
    TabbedContent { margin: 1 2 0 2; height: 1fr; }
    TabPane { padding: 1 2; background: #0c1427; }
    VerticalScroll { height: 1fr; }
    Label { margin-top: 1; color: #9db3d5; }
    Input, TextArea { border: round #304a76; background: #0a1222; }
    Input:focus, TextArea:focus { border: round #4d9cff; }
    TextArea { height: 12; }
    .hint { color: #7890b5; margin-bottom: 1; }
    #actions { height: 4; align-horizontal: right; padding: 0 2; }
    #actions Button { margin-left: 1; }
    #status { height: 3; margin: 0 2; padding: 0 1; color: #9db3d5; }
    #review { border: round #304a76; padding: 1 2; background: #0a1222; }
    """

    def __init__(self, output_root: Path) -> None:
        super().__init__()
        self.output_root = output_root.resolve()

    def compose(self) -> ComposeResult:
        yield Header()
        with TabbedContent(initial="identity"):
            with TabPane("1 · Identity", id="identity"):
                with VerticalScroll():
                    yield Label("Workflow name")
                    yield Input("my-solver", id="name")
                    yield Static("Creates examples/example-my-solver. Use lowercase letters, numbers, and hyphens.", classes="hint")
                    yield Label("Cluster SSH host")
                    yield Input(DEFAULT_HOST, id="host")
                    yield Static("An SSH config alias or host name; SLURMY_HOST supplies the initial value.", classes="hint")
                    yield Label("Slurm partition")
                    yield Input("CPU-amd", id="partition")
                    yield Label("Problem globs — one per line")
                    yield TextArea("problems/**/*.p", id="problems")
                    yield Static("Add the matching benchmark files to the generated problems/ directory.", classes="hint")
            with TabPane("2 · Build", id="build"):
                with VerticalScroll():
                    yield Label("Artifact path")
                    yield Input("my-solver", id="artifact")
                    yield Static("Relative to SLURMY_BUILD_OUTPUT; the recipe must install an executable here.", classes="hint")
                    yield Label("Build commands")
                    yield TextArea(
                        'git clone --depth 1 https://github.com/OWNER/PROJECT.git "$SLURMY_BUILD_WORK/source"\n'
                        'make -C "$SLURMY_BUILD_WORK/source" -j"${SLURM_CPUS_PER_TASK:-1}"\n'
                        'install -m 0755 "$SLURMY_BUILD_WORK/source/my-solver" "$SLURMY_BUILD_OUTPUT/my-solver"',
                        language="bash",
                        id="build-body",
                    )
                    yield Static("These commands run on one compute node. Clone/download source, compile it, and install the artifact.", classes="hint")
                    yield Label("Build CPUs · memory · time")
                    with Horizontal():
                        yield Input("8", id="build-cpus")
                        yield Input("8GiB", id="build-memory")
                        yield Input("00:30:00", id="build-time")
            with TabPane("3 · Experiment", id="experiment"):
                with VerticalScroll():
                    yield Label("Solver invocation")
                    yield Input("./my-solver --time-limit {{cpu-limit}} {{problem}}", id="solver-command")
                    yield Static("Use {{problem}} plus any of {{cpu-limit}}, {{wc-limit}}, and {{mem-limit}}.", classes="hint")
                    yield Label("Per-call CPU limit · wall limit · memory limit")
                    with Horizontal():
                        yield Input("60", id="cpu-limit")
                        yield Input("70", id="wall-limit")
                        yield Input("2GiB", id="memory-limit")
                    yield Label("Slurm CPU request · memory request")
                    with Horizontal():
                        yield Input("1-core", id="cpu-request")
                        yield Input("2300MiB", id="memory-request")
                    yield Label("Calls per array task · maximum simultaneous array tasks · worker overhead seconds")
                    with Horizontal():
                        yield Input("20", id="batch-size")
                        yield Input("100", id="max-parallel")
                        yield Input("60", id="worker-overhead")
            with TabPane("4 · Review", id="review-tab"):
                yield Static(id="review")
        with Horizontal(id="actions"):
            yield Button("Review", id="review-button")
            yield Button("Generate workflow", variant="primary", id="generate-button")
            yield Button("Quit", id="quit-button")
        yield Static("Fill in the four tabs. Nothing is written until you choose Generate workflow.", id="status")
        yield Footer()

    def field(self, selector: str) -> str:
        return self.query_one(selector, Input).value.strip()

    def workflow(self) -> Workflow:
        patterns = tuple(
            line.strip()
            for line in self.query_one("#problems", TextArea).text.splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        )
        workflow = Workflow(
            name=self.field("#name").removeprefix("example-"),
            host=self.field("#host"),
            partition=self.field("#partition"),
            artifact=self.field("#artifact"),
            solver_command=self.field("#solver-command"),
            problem_globs=patterns,
            build_body=self.query_one("#build-body", TextArea).text,
            cpu_limit=positive_integer(self.field("#cpu-limit"), "CPU limit"),
            wall_limit=positive_integer(self.field("#wall-limit"), "wall limit"),
            memory_limit=self.field("#memory-limit"),
            cpu_request=self.field("#cpu-request"),
            memory_request=self.field("#memory-request"),
            batch_size=positive_integer(self.field("#batch-size"), "batch size"),
            max_parallel=positive_integer(self.field("#max-parallel"), "maximum parallel tasks"),
            worker_overhead=positive_integer(self.field("#worker-overhead"), "worker overhead", zero_allowed=True),
            build_cpus=positive_integer(self.field("#build-cpus"), "build CPUs"),
            build_memory=self.field("#build-memory"),
            build_time=self.field("#build-time"),
        )
        workflow.validate()
        return workflow

    def update_review(self) -> None:
        try:
            workflow = self.workflow()
        except WorkflowError as exc:
            self.query_one("#review", Static).update(f"[bold #ff7373]Needs attention[/]\n\n{exc}")
            return
        destination = self.output_root / f"example-{workflow.name}"
        self.query_one("#review", Static).update(
            "\n".join(
                [
                    f"[bold #75a9ff]{workflow.name}[/]  →  {destination}",
                    f"Cluster: [bold]{workflow.host}[/] · partition: [bold]{workflow.partition}[/]",
                    f"Artifact: bin/{workflow.artifact}",
                    f"Problems: {len(workflow.problem_globs)} glob(s)",
                    f"Limits: {workflow.cpu_limit}s CPU · {workflow.wall_limit}s wall · {workflow.memory_limit}",
                    f"Array: {workflow.batch_size} calls/task · at most {workflow.max_parallel} tasks running",
                    "\nGenerated files: Makefile, workflow.mk, build.sh, solver.solver, README.md, problems/",
                ]
            )
        )

    def action_generate(self) -> None:
        try:
            destination = generate_workflow(self.workflow(), self.output_root)
        except (WorkflowError, OSError) as exc:
            self.query_one("#status", Static).update(f"[bold #ff7373]Could not generate:[/] {exc}")
            self.notify(str(exc), severity="error")
            return
        self.query_one("#status", Static).update(
            f"[bold #56d69a]Created {destination}[/] — add problems, review the files, then run make there."
        )
        self.notify(f"Created {destination.name}", title="Workflow ready")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "quit-button":
            self.exit()
        elif event.button.id == "review-button":
            self.update_review()
            self.query_one(TabbedContent).active = "review-tab"
        elif event.button.id == "generate-button":
            self.action_generate()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="example-maker.py",
        description="Open an interactive Textual wizard that creates an example-* workflow.",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="directory in which example-NAME is created (default: examples directory)",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    ExampleMakerApp(args.output_root).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
