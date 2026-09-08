#!/usr/bin/env python
"""Ask one question at a time and create a Slurmy experiment workflow."""

from __future__ import annotations

from dataclasses import dataclass
import glob
import os
from pathlib import Path
import re
import shutil
import sys
from typing import Any

try:
    from rich.markup import escape
    from textual.app import App, ComposeResult
    from textual.containers import Horizontal, VerticalScroll
    from textual.widgets import Button, Footer, Header, Input, Label, Static, TextArea
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


SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
SIMPLE_RE = re.compile(r"^[A-Za-z0-9._-]+$")
MEMORY_RE = re.compile(
    r"^[1-9][0-9]*(?:MB|MiB|GB|GiB|TB|TiB|M|G|T)?$", re.IGNORECASE
)
CPU_REQUESTS = {"1-core", "4-core", "8-core", "16-core", "32-core", "64-core"}
PROBLEM_PLACEHOLDER_RE = re.compile(r"\{\{problem(?:=[^{}]+)?\}\}")


class WorkflowError(ValueError):
    """A wizard answer or generated workflow is invalid."""


@dataclass(frozen=True)
class Workflow:
    name: str
    host: str
    partition: str
    solver_mode: str
    solver_files: tuple[str, ...]
    solver_root: str | None
    solver_commands: tuple[str, ...]
    problem_globs: tuple[str, ...]
    cpu_limit: int
    wall_limit: int
    memory_limit: str
    cpu_request: str
    memory_request: str
    batch_size: int
    max_parallel: int
    worker_overhead: int


@dataclass(frozen=True)
class Question:
    key: str
    title: str
    explanation: str
    hint: str
    multiline: bool = False


INITIAL_QUESTIONS = (
    Question(
        "name",
        "What should this workflow be called?",
        "This creates a sibling directory named example-NAME.",
        "Use lowercase letters, numbers, and hyphens, for example: superposition-study",
    ),
    Question(
        "host",
        "What SSH host reaches the cluster?",
        "Enter an existing SSH config alias or host name.",
        "For the TU cluster this is usually: datalab",
    ),
    Question(
        "partition",
        "Which Slurm partition should run the experiment?",
        "Slurm will select compute nodes from this partition.",
        "For example: CPU-amd",
    ),
    Question(
        "solver_mode",
        "Use an existing solver-description file or create one interactively?",
        "Choose whether to reference existing description files or create one interactively.",
        "Enter exactly: existing or create",
    ),
)

EXISTING_SOLVER_QUESTIONS = (
    Question(
        "solver_files",
        "Where are the solver description files?",
        "Enter one path or glob per line. Every glob is checked now, and every matched .solver-style description must name an existing solver root.",
        "Relative paths start from examples/example-maker/. All matches are stored as absolute paths.",
        multiline=True,
    ),
)

CREATE_SOLVER_QUESTIONS = (
    Question(
        "solver_root",
        "Where is the solver root directory?",
        "This existing directory should contain the executable and every runtime file the solver needs.",
        "Relative paths start from examples/example-maker/. The stored path will be absolute.",
    ),
    Question(
        "solver_commands",
        "How should the solver be invoked?",
        "Enter one complete invocation per line. Each invocation becomes a configuration in the generated solver-description file.",
        "Every line must contain {{problem}} or {{problem=path}}; limit variables such as {{cpu-limit}} are supported.",
        multiline=True,
    ),
)

FINAL_QUESTIONS = (
    Question(
        "problem_globs",
        "Where are the benchmark problems?",
        "Enter one path or glob per line. Every glob must match at least one existing regular file before you can continue.",
        "Relative paths start from examples/example-maker/. Absolute globs are stored and packaged only when the submission runs.",
        multiline=True,
    ),
    Question(
        "cpu_limit",
        "How many CPU seconds may each solver call use?",
        "runsolver enforces this limit separately for every call.",
        "Enter a positive whole number of seconds.",
    ),
    Question(
        "wall_limit",
        "How many wall-clock seconds may each solver call use?",
        "This covers elapsed real time, including time when a process is not on a CPU.",
        "Enter a positive whole number of seconds.",
    ),
    Question(
        "memory_limit",
        "How much memory may each solver call use?",
        "runsolver enforces this memory limit on the solver process tree.",
        "For example: 2GiB or 2300MiB",
    ),
    Question(
        "cpu_request",
        "How many CPUs should each Slurm array task request?",
        "This is the allocation for one array task, which runs its calls sequentially.",
        "Choose one of: 1-core, 4-core, 8-core, 16-core, 32-core, 64-core",
    ),
    Question(
        "memory_request",
        "How much memory should each Slurm array task request?",
        "Leave some space above the solver memory limit for Bash and runsolver.",
        "For example: 2300MiB",
    ),
    Question(
        "batch_size",
        "How many solver calls should one array task run?",
        "Calls within one array task run sequentially.",
        "Enter a positive whole number.",
    ),
    Question(
        "max_parallel",
        "How many array tasks may run simultaneously?",
        "This is the maximum number of this workflow's Slurm array tasks running at once across all cluster nodes.",
        "Enter a positive whole number.",
    ),
    Question(
        "worker_overhead",
        "How many overhead seconds should each array task receive?",
        "This extra Slurm time covers staging and saving results around the solver calls.",
        "Enter zero or a positive whole number of seconds.",
    ),
)


def lines(value: str) -> tuple[str, ...]:
    return tuple(
        line.strip()
        for line in value.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    )


def absolute_pattern(value: str, base: Path) -> str:
    expanded = os.path.expandvars(os.path.expanduser(value))
    if not os.path.isabs(expanded):
        expanded = os.path.join(base, expanded)
    return os.path.abspath(expanded)


def expand_regular_files(
    value: str, *, base: Path, description: str
) -> tuple[tuple[str, ...], tuple[str, ...]]:
    patterns = lines(value)
    if not patterns:
        raise WorkflowError(f"Enter at least one {description} path or glob.")
    normalized: list[str] = []
    matched_files: list[str] = []
    seen: set[str] = set()
    for raw_pattern in patterns:
        if "\x00" in raw_pattern or "\n" in raw_pattern:
            raise WorkflowError(f"Invalid {description} path: {raw_pattern!r}")
        pattern = absolute_pattern(raw_pattern, base)
        matches = sorted(
            path for path in glob.glob(pattern, recursive=True) if os.path.isfile(path)
        )
        if not matches:
            raise WorkflowError(
                f"This {description} pattern matches no regular files:\n{pattern}"
            )
        normalized.append(pattern)
        for match in matches:
            absolute = os.path.abspath(match)
            if absolute not in seen:
                seen.add(absolute)
                matched_files.append(absolute)
    return tuple(normalized), tuple(matched_files)


def validate_solver_file(filename: str) -> None:
    path = Path(filename)
    try:
        content = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as exc:
        raise WorkflowError(f"Cannot read solver description {path}: {exc}") from exc
    if not content or not content[0].strip():
        raise WorkflowError(f"Solver description has no root on its first line: {path}")
    root = Path(os.path.expanduser(content[0].strip()))
    if not root.is_absolute():
        root = path.parent / root
    root = Path(os.path.abspath(root))
    if not root.is_dir():
        raise WorkflowError(
            f"Solver description {path} names a root that is not an existing directory:\n{root}"
        )
    commands = [
        line for line in content[1:] if line.strip() and not line.lstrip().startswith("#")
    ]
    if not commands:
        raise WorkflowError(f"Solver description has no invocation lines: {path}")


def existing_directory(value: str, *, base: Path, description: str) -> str:
    if "\x00" in value or "\n" in value:
        raise WorkflowError(f"Invalid {description} path: {value!r}")
    path = absolute_pattern(value, base)
    if glob.has_magic(path):
        raise WorkflowError(f"Enter one {description} directory, not a glob.")
    if not os.path.isdir(path):
        raise WorkflowError(f"This {description} directory does not exist:\n{path}")
    return path


def positive_integer(value: str, description: str, *, zero_allowed: bool = False) -> int:
    try:
        number = int(value)
    except ValueError as exc:
        raise WorkflowError(f"{description} must be a whole number.") from exc
    if number < 0 or (number == 0 and not zero_allowed):
        qualifier = "zero or greater" if zero_allowed else "greater than zero"
        raise WorkflowError(f"{description} must be {qualifier}.")
    return number


def workflow_makefile(workflow: Workflow) -> str:
    return "\n".join(
        [
            "# Generated by example-maker.py. Input lists contain absolute paths.",
            "# Solver installations and benchmark problems were not copied here.",
            f"SLURMY_HOST := {workflow.host}",
            f"SLURMY_BUILD_PARTITION := {workflow.partition}",
            f"EXAMPLE_NAME := {workflow.name}",
            "SOLVER_PATHS_FILE := $(CURDIR)/solver-paths.txt",
            "PROBLEM_GLOBS_FILE := $(CURDIR)/problem-globs.txt",
            f"CPU_LIMIT := {workflow.cpu_limit}",
            f"WALL_LIMIT := {workflow.wall_limit}",
            f"MEMORY_LIMIT := {workflow.memory_limit}",
            f"CPU_REQUEST := {workflow.cpu_request}",
            f"MEMORY_REQUEST := {workflow.memory_request}",
            f"BATCH_SIZE := {workflow.batch_size}",
            f"MAX_PARALLEL := {workflow.max_parallel}",
            f"WORKER_OVERHEAD := {workflow.worker_overhead}",
            "",
        ]
    )


def generated_readme(workflow: Workflow) -> str:
    if workflow.solver_mode == "create":
        solver_note = """The wizard created `solver.solver`, which references the preexisting
solver root you selected. The solver installation itself was not copied here."""
    else:
        solver_note = """This workflow references preexisting solver-description files.
They were not copied into this directory."""
    return f"""# {workflow.name} Slurmy workflow

{solver_note} The benchmark files are also referenced in their preexisting
locations. `slurmy.py` reads these absolute paths when `make` runs, and the
generated `submit.sh` packages the solver root and matched problems when the
job is submitted.

Review `workflow.mk`, then run:

```bash
make
make submit
make monitor   # Or use make sync to download results.
```

If referenced files move, update `solver-paths.txt` or `problem-globs.txt`
before running `make` again.
"""


def generate_workflow(workflow: Workflow) -> Path:
    repository = Path(__file__).resolve().parents[2]
    template = repository / "examples" / "example-template" / "Makefile"
    destination = repository / "examples" / f"example-{workflow.name}"
    if destination.exists():
        raise WorkflowError(f"Destination already exists: {destination}")
    if not template.is_file():
        raise WorkflowError(f"Template Makefile is missing: {template}")

    staging = destination.with_name(f".{destination.name}.creating-{os.getpid()}")
    if staging.exists():
        raise WorkflowError(f"Temporary destination already exists: {staging}")
    try:
        staging.mkdir()
        shutil.copy2(template, staging / "Makefile")
        (staging / "workflow.mk").write_text(
            workflow_makefile(workflow), encoding="utf-8"
        )
        (staging / "README.md").write_text(
            generated_readme(workflow), encoding="utf-8"
        )
        solver_files = workflow.solver_files
        if workflow.solver_mode == "create":
            if workflow.solver_root is None or not workflow.solver_commands:
                raise WorkflowError("The new solver description is incomplete.")
            (staging / "solver.solver").write_text(
                "\n".join((workflow.solver_root, *workflow.solver_commands)) + "\n",
                encoding="utf-8",
            )
            solver_files = (str(destination / "solver.solver"),)
        (staging / "solver-paths.txt").write_text(
            "\n".join(solver_files) + "\n", encoding="utf-8"
        )
        (staging / "problem-globs.txt").write_text(
            "\n".join(workflow.problem_globs) + "\n", encoding="utf-8"
        )
        staging.rename(destination)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return destination


class ExampleMakerApp(App[None]):
    TITLE = "Slurmy Example Maker"
    SUB_TITLE = "One question at a time"
    BINDINGS = [
        ("ctrl+enter", "next_question", "Continue"),
        ("ctrl+b", "previous_question", "Back"),
        ("ctrl+q", "quit", "Quit"),
    ]

    CSS = """
    Screen { background: #09101f; color: #dce7f7; }
    Header { background: #101b33; }
    #page { width: 1fr; height: 1fr; margin: 2 2 0 2; }
    #progress { color: #75a9ff; margin-bottom: 1; }
    #question { text-style: bold; color: #ffffff; margin-bottom: 1; }
    #explanation { color: #b6c8e2; margin-bottom: 1; }
    #hint { color: #7890b5; margin-bottom: 1; }
    Input, TextArea { border: round #304a76; background: #0a1222; }
    Input:focus, TextArea:focus { border: round #4d9cff; }
    Input { height: 3; }
    TextArea { height: 12; }
    #validation { min-height: 3; margin-top: 1; color: #9db3d5; }
    #review { display: none; border: round #304a76; padding: 1 2; background: #0a1222; }
    #actions { height: 5; width: 1fr; margin: 0 2; align-horizontal: right; }
    #actions Button { margin-left: 1; }
    """

    def __init__(self) -> None:
        super().__init__()
        self.index = 0
        self.answers: dict[str, Any] = {}
        self.maker_directory = Path(__file__).resolve().parent

    def questions(self) -> tuple[Question, ...]:
        mode = self.answers.get("solver_mode")
        if mode == "existing":
            solver_questions = EXISTING_SOLVER_QUESTIONS
        elif mode == "create":
            solver_questions = CREATE_SOLVER_QUESTIONS
        else:
            solver_questions = ()
        return INITIAL_QUESTIONS + solver_questions + FINAL_QUESTIONS

    def compose(self) -> ComposeResult:
        yield Header()
        with VerticalScroll(id="page"):
            yield Static(id="progress")
            yield Label(id="question")
            yield Static(id="explanation")
            yield Static(id="hint")
            yield Input(id="single-answer")
            yield TextArea(id="multiple-answer")
            yield Static(id="validation")
            yield Static(id="review")
        with Horizontal(id="actions"):
            yield Button("Back", id="back")
            yield Button("Continue", variant="primary", id="continue")
            yield Button("Create workflow", variant="success", id="create")
            yield Button("Quit", id="quit")
        yield Footer()

    def on_mount(self) -> None:
        self.show_question()

    def raw_answer(self) -> str:
        question = self.questions()[self.index]
        if question.multiline:
            return self.query_one("#multiple-answer", TextArea).text.strip()
        return self.query_one("#single-answer", Input).value.strip()

    def validate_answer(self, question: Question, value: str) -> tuple[Any, str]:
        if not value:
            raise WorkflowError("An answer is required; this wizard has no defaults.")
        if question.key == "name":
            name = value.removeprefix("example-")
            if not SLUG_RE.fullmatch(name):
                raise WorkflowError("Use lowercase letters, numbers, and hyphens only.")
            destination = Path(__file__).resolve().parents[1] / f"example-{name}"
            if destination.exists():
                raise WorkflowError(f"That workflow already exists: {destination}")
            return name, f"Will create {destination}"
        if question.key in {"host", "partition"}:
            if not SIMPLE_RE.fullmatch(value):
                raise WorkflowError("Use only letters, numbers, dots, underscores, and hyphens.")
            return value, "Accepted."
        if question.key == "solver_mode":
            mode = value.lower()
            if mode not in {"existing", "create"}:
                raise WorkflowError("Enter exactly: existing or create")
            return mode, "Accepted."
        if question.key == "solver_files":
            _, matches = expand_regular_files(
                value, base=self.maker_directory, description="solver"
            )
            for filename in matches:
                validate_solver_file(filename)
            return matches, f"Validated {len(matches)} solver description file(s)."
        if question.key == "solver_root":
            root = existing_directory(
                value, base=self.maker_directory, description="solver root"
            )
            return root, f"Validated solver root: {root}"
        if question.key == "solver_commands":
            commands = lines(value)
            if not commands:
                raise WorkflowError("Enter at least one solver invocation.")
            for command in commands:
                if not PROBLEM_PLACEHOLDER_RE.search(command):
                    raise WorkflowError(
                        "Every invocation must contain {{problem}} or {{problem=path}}:\n"
                        + command
                    )
            return commands, f"Validated {len(commands)} solver invocation(s)."
        if question.key == "problem_globs":
            patterns, matches = expand_regular_files(
                value, base=self.maker_directory, description="problem"
            )
            return patterns, f"Validated {len(matches)} problem file(s) from {len(patterns)} glob(s)."
        if question.key in {"cpu_limit", "wall_limit", "batch_size", "max_parallel"}:
            return positive_integer(value, question.title), "Accepted."
        if question.key == "worker_overhead":
            return positive_integer(value, question.title, zero_allowed=True), "Accepted."
        if question.key in {"memory_limit", "memory_request"}:
            if not MEMORY_RE.fullmatch(value):
                raise WorkflowError("Use a positive value such as 2GiB or 2300MiB.")
            return value, "Accepted."
        if question.key == "cpu_request":
            if value not in CPU_REQUESTS:
                raise WorkflowError("Choose one of: " + ", ".join(sorted(CPU_REQUESTS)))
            return value, "Accepted."
        raise WorkflowError(f"No validator exists for {question.key}.")

    def save_current_answer(self) -> bool:
        question = self.questions()[self.index]
        try:
            answer, message = self.validate_answer(question, self.raw_answer())
        except WorkflowError as exc:
            self.query_one("#validation", Static).update(
                f"[bold #ff7373]Please correct this answer:[/] {escape(str(exc))}"
            )
            self.notify(str(exc), severity="error")
            return False
        if question.key == "solver_mode" and answer != self.answers.get("solver_mode"):
            for key in ("solver_files", "solver_root", "solver_commands"):
                self.answers.pop(key, None)
                self.answers.pop(f"_{key}_raw", None)
        self.answers[question.key] = answer
        self.answers[f"_{question.key}_raw"] = self.raw_answer()
        self.query_one("#validation", Static).update(
            f"[bold #56d69a]✓ {message}[/]"
        )
        if question.key in {"solver_files", "solver_root", "problem_globs"}:
            self.notify(message, title="Paths validated")
        return True

    def show_question(self) -> None:
        questions = self.questions()
        question = questions[self.index]
        self.query_one("#progress", Static).update(
            f"Question {self.index + 1} of {len(questions)}"
        )
        self.query_one("#question", Label).update(question.title)
        self.query_one("#explanation", Static).update(question.explanation)
        self.query_one("#hint", Static).update(question.hint)
        single = self.query_one("#single-answer", Input)
        multiple = self.query_one("#multiple-answer", TextArea)
        single.display = not question.multiline
        multiple.display = question.multiline
        raw = self.answers.get(f"_{question.key}_raw", "")
        single.value = raw if not question.multiline else ""
        multiple.load_text(raw if question.multiline else "")
        self.query_one("#validation", Static).update("")
        self.query_one("#review", Static).display = False
        self.query_one("#create", Button).display = False
        continue_button = self.query_one("#continue", Button)
        continue_button.display = True
        continue_button.label = (
            "Validate paths & continue"
            if question.key in {"solver_files", "solver_root", "problem_globs"}
            else "Continue"
        )
        self.query_one("#back", Button).disabled = self.index == 0
        (multiple if question.multiline else single).focus()

    def workflow(self) -> Workflow:
        missing = [
            question.key for question in self.questions() if question.key not in self.answers
        ]
        if missing:
            raise WorkflowError("Unanswered questions: " + ", ".join(missing))
        return Workflow(
            name=self.answers["name"],
            host=self.answers["host"],
            partition=self.answers["partition"],
            solver_mode=self.answers["solver_mode"],
            solver_files=self.answers.get("solver_files", ()),
            solver_root=self.answers.get("solver_root"),
            solver_commands=self.answers.get("solver_commands", ()),
            problem_globs=self.answers["problem_globs"],
            cpu_limit=self.answers["cpu_limit"],
            wall_limit=self.answers["wall_limit"],
            memory_limit=self.answers["memory_limit"],
            cpu_request=self.answers["cpu_request"],
            memory_request=self.answers["memory_request"],
            batch_size=self.answers["batch_size"],
            max_parallel=self.answers["max_parallel"],
            worker_overhead=self.answers["worker_overhead"],
        )

    def show_review(self) -> None:
        workflow = self.workflow()
        destination = Path(__file__).resolve().parents[1] / f"example-{workflow.name}"
        self.query_one("#progress", Static).update("Review")
        self.query_one("#question", Label).update("Ready to create the workflow")
        self.query_one("#explanation", Static).update(
            "No solver installation or problem files will be copied. The generated workflow keeps absolute references to the validated locations below."
        )
        self.query_one("#hint", Static).update("")
        self.query_one("#single-answer", Input).display = False
        self.query_one("#multiple-answer", TextArea).display = False
        self.query_one("#validation", Static).update("")
        review = self.query_one("#review", Static)
        review.display = True
        if workflow.solver_mode == "existing":
            solver_lines = [
                f"Solver descriptions: {len(workflow.solver_files)}",
                *(f"  {escape(filename)}" for filename in workflow.solver_files),
            ]
        else:
            solver_lines = [
                f"New solver description: {escape(str(destination / 'solver.solver'))}",
                f"Solver root: {escape(workflow.solver_root or '')}",
                "Invocations:",
                *(f"  {escape(command)}" for command in workflow.solver_commands),
            ]
        review.update(
            "\n".join(
                [
                    f"[bold #75a9ff]{escape(str(destination))}[/]",
                    f"Cluster: {escape(workflow.host)} · partition: {escape(workflow.partition)}",
                    *solver_lines,
                    f"Problem globs: {len(workflow.problem_globs)}",
                    *(f"  {escape(pattern)}" for pattern in workflow.problem_globs),
                    f"Limits: {workflow.cpu_limit}s CPU · {workflow.wall_limit}s wall · {workflow.memory_limit}",
                    f"Allocation: {workflow.cpu_request} · {workflow.memory_request}",
                    f"Array: {workflow.batch_size} calls/task · {workflow.max_parallel} simultaneous tasks",
                ]
            )
        )
        self.query_one("#continue", Button).display = False
        self.query_one("#create", Button).display = True
        self.query_one("#back", Button).disabled = False

    def action_next_question(self) -> None:
        if self.query_one("#create", Button).display:
            return
        if not self.save_current_answer():
            return
        if self.index == len(self.questions()) - 1:
            self.show_review()
        else:
            self.index += 1
            self.show_question()

    def action_previous_question(self) -> None:
        if self.query_one("#create", Button).display:
            self.show_question()
            return
        if self.index > 0:
            self.index -= 1
            self.show_question()

    def create_workflow(self) -> None:
        try:
            destination = generate_workflow(self.workflow())
        except (WorkflowError, OSError) as exc:
            self.query_one("#validation", Static).update(
                f"[bold #ff7373]Could not create workflow:[/] {escape(str(exc))}"
            )
            self.notify(str(exc), severity="error")
            return
        self.query_one("#review", Static).update(
            f"[bold #56d69a]Created {escape(str(destination))}[/]\n\n"
            "No solver installation or problem files were copied. Review workflow.mk, then run make."
        )
        self.query_one("#create", Button).disabled = True
        self.notify(f"Created {destination.name}", title="Workflow ready")

    def on_input_submitted(self, _event: Input.Submitted) -> None:
        self.action_next_question()

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "back":
            self.action_previous_question()
        elif event.button.id == "continue":
            self.action_next_question()
        elif event.button.id == "create":
            self.create_workflow()
        elif event.button.id == "quit":
            self.exit()


def main() -> int:
    ExampleMakerApp().run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
