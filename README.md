<div align="center">
  <img src="implementation/logo.svg" alt="Slurmy logo" width="240">

# Slurmy

</div>

Create theorem-proving workflows, build their solvers and resource limiters on
a Slurm cluster, and inspect each job's progress and output in a local web app.

A **workflow** is a reusable definition: solver commands, problem selection,
build recipes, limits, and cluster settings. Dispatching a workflow creates a
**job**, containing explicit solver–problem calls grouped into Slurm batches.

```bash
python -m pip install -r requirements.txt
python web-app/main.py  # Opens the browser; Ctrl-C stops the local server.
```

Open **Workflows** to browse the supplied examples, or **New workflow** to
create your own. User workflows live directly in `workflows/my-workflows/NAME/`.
See [example workflows](workflows/example-workflows/README.md) for Vampire, E, and Drodi.

Python runs on your computer. Building and proving run on the cluster, so your
computer and the cluster can use different architectures.

The repository is organized around `workflows/`: `example-problems/` holds
sample TPTP inputs, `example-solvers/` holds solver source snapshots,
`example-workflows/` holds runnable examples, and `my-workflows/` holds your
private workflow definitions. The local UI lives in `web-app/`; runner scripts
and submission templates live in `implementation/`. Remote build tooling is
in `implementation/build/`, and downloaded results go to `job-results/`.

<details>
<summary><strong>🚀 Requirements and setup</strong></summary>

<blockquote>

Locally: Python 3.9+, Bash, make, SSH/SCP, rsync, and GNU or BSD tar.
Linux and macOS (including Apple Silicon) work with the same interface; use
WSL on Windows. Native Windows shells are not supported.

```bash
# Optional: isolate Python dependencies in a virtual environment.
python -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

The scripts and Makefiles use whichever `python` is on your PATH.
On macOS, `brew install python rsync` supplies those tools; if make is missing,
install Apple's command-line tools with `xcode-select --install`.
Use Bash for the shell examples. The built-in macOS Bash and tar suffice;
no GNU tar replacement or local C++ compiler is needed.

Configure non-interactive SSH access, then choose a partition:

```bash
export SLURMY_HOST=datalab       # SSH config alias; datalab is the default.
export SLURMY_PARTITION=CPU-amd  # Selects the cluster's CPU-amd set of machines.
ssh "$SLURMY_HOST" hostname
```

The core three-file interface uses these environment variables.
Monitor, sync, cancel, and the standalone builder also accept `--host`, which
overrides `SLURMY_HOST`. Example Makefiles accept the same variables.

The cluster must provide Linux, Bash 4+, Slurm, tar, rsync, GNU `timeout`,
and util-linux (`lscpu`, `taskset`, `setsid`).
The scheduler currently requires a homogeneous partition, core-based allocation
(`CR_CORE` or `CR_CORE_MEMORY`), and `task/cgroup` with
`ConstrainCores=yes`. Forced core-sharing partitions are rejected.
These are administrator settings, not laptop prerequisites.

Build recipes additionally need their normal build tools on compute nodes
(e.g. Git, CMake, make, GCC/G++, curl, and source-download access).
Binaries are built and run on the cluster; the laptop architecture may differ.

</blockquote>
</details>

<details>
<summary><strong>🌐 Create workflows and run jobs in the browser</strong></summary>

<blockquote>

The guided setup follows **Basics → Solvers → Problems → Limiter → Review and Slurm Settings**.

- **Basics:** name the workflow and choose the SSH host and Slurm partition.
- **Solvers:** each solver has a runtime root (containing its executable).
  To build on the cluster, provide its source directory and Bash build script;
  the executable path is inferred from the solver command. In interactive mode,
  **Copy from workflow…** adds an editable solver configuration (including its
  build settings) from an example or one of your saved workflows.
- **Problems:** select files, globs, or ZIP/tar archives. Their union is paired with every
  configuration; duplicate problem paths are removed. An explicit jobpairs CSV
  already supplies its problems. The shared TPTP library at
  `/share/Slurmy-TPTP-v9.2.1` is selected by default; custom axiom globs replace it.
- **Limiter:** runsolver and its remote build are preconfigured. You may change
  the invocation, resource root, or build recipe. New workflows include
  `--timestamp` in the runsolver invocation so solver-output lines carry timing
  prefixes. Both invocation sections have collapsible
  placeholder references and editable examples.
- **Review and Slurm Settings:** choose Problem Major, Solver Major, or random
  jobpair order, set jobpairs per Slurm task (batch size), inspect the generated
  files, then save.
  Saving does not build or submit anything.

Path fields have a local file browser and are checked when you leave them.
Relative paths entered in the form resolve from `workflows/my-workflows/`; paths in imported
files resolve from those files' directories. Saved call/resource paths are
absolute. Selecting a path does not upload or copy its contents.

On a workflow page:

| Action | Effect |
| --- | --- |
| Prepare & dispatch job (main button) | Refresh submission scripts, build declared resources on compute nodes, download them, then submit the batches. |
| Dispatch prepared job (arrow menu) | Use the existing `submit.sh` and helper files as-is; do not regenerate them. |
| Prepare only (arrow menu) | Create or refresh submission scripts locally without dispatching. |
| Edit settings | Revisit the saved form choices, including the name. |
| Duplicate workflow | Create an independent user workflow from an example or an existing workflow. |
| Delete workflow | Move a user workflow to `workflows/my-workflows/.deleted-workflows/` for recovery. |

Examples are read-only; duplicate one to customize it. Renaming a user workflow
moves its folder while keeping earlier jobs linked. Local definitions and past
jobs appear separately: **Workflows** contains templates, **Job history**
contains their particular runs.

The job page has a collapsible results overview with a pie chart and percentages
for every call outcome, labeled by whether the status came from the solver or
resource limiter. Select a status to compare solvers, or open the interactive
3D comparison to see counts for every solver and status together; an exact-count
table is available beneath it. The page also shows searchable, sortable calls
and progress. Select a completed
call to inspect solver stdout/stderr, limiter output, watcher measurements, and
controller diagnostics, including the exact solver and limiter commands.
Ordinary time or memory limits and SZS answers such as `Satisfiable` or
`CounterSatisfiable` are outcomes, not execution issues, even when a solver
uses a nonzero exit code for its answer. Crashes and SZS error statuses remain
issues. If runsolver's variables disagree with its watcher log
about a memory-limit kill, the watcher log takes precedence.
Percent complete counts finished calls; it does not estimate progress within
a running proof search. **Sync results** downloads the saved results once;
**Cancel** lets you select an active Slurm ID to cancel.

The app provides a dark-mode toggle and collapsible section headers.
Workflow pages list linked jobs; older submissions without a workflow path
remain available under Job history.
You can optionally name a job when dispatching it, then rename it from its job
page or the jobs table once its Slurm allocations finish. A new job uses that
name as the prefix of its ID, cluster directory, and Slurm batch names; its
full name is stored in the job's `metadata.json`. Renaming a finished job
moves its cluster directory and any locally synced results and updates their
metadata. Slurm's numeric allocation IDs and historical accounting records do
not change.

On a job page, **Sync/Download** fetches new results without re-downloading
unchanged files. Problem names open a page with the packaged problem text and
that job's calls on the problem, fastest first. Call status shows a reported
SZS status when available; otherwise it distinguishes normal completion,
resource limits, solver failures, and limiter failures. **Delete job** is
available after its Slurm allocations finish; it permanently removes the
cluster job directory and any locally synced results when clicked. The same
action is available in the Job history table.
Calls without a saved result show whether their batch is still queued or
running, ended without a result, or cannot be located in Slurm accounting.
The **Will run?** and **Reason** columns explain what is known; an available
batch log can be inspected from that call. “Unknown” does not imply a retry.

<details>
<summary><strong>🖥️ Starting and stopping the app</strong></summary>

<blockquote>

```bash
python web-app/main.py                  # Foreground; Ctrl-C stops this server.
python web-app/main.py --new            # Open New workflow.
python web-app/main.py --directory workflows/example-workflows/example-vampire
python web-app/main.py --job JOB_ID     # Open one submitted job.
python web-app/main.py --background     # Start/reuse the server and return to the shell.
python web-app/main.py --stop           # Stop the app; Slurm jobs continue.
```

Repeated invocations reuse an existing server. `make monitor` in a workflow
starts/reuses it in the background and opens that workflow's page.
Closing a browser tab does not stop the server. Restart after updating the code
or dependencies. Let active build/submission operations finish first;
`--stop` refuses while a web-launched operation is running.

The app uses Flask and Waitress, listens only on `127.0.0.1` (normally port
8765), and is intended for one local user. It can read local paths and execute
trusted workflow commands with your permissions; do not expose it through a
public tunnel, proxy, or shared server. Cross-site and unprotected state-changing
requests are rejected. No database, CDN, or JavaScript build tools are required.

`--host` / `SLURMY_HOST` chooses the SSH alias.
`--port` / `SLURMY_WEB_PORT` chooses a local web port.
`--no-browser` suppresses automatic browser opening. The legacy
`slurmy-monitor.py` opens this same web app.

Server and operation logs live in the ignored `.slurmy-web/` directory.
Operations are tracked in memory. On Windows, run Slurmy inside WSL and open
the printed localhost URL in your Windows browser if automatic opening fails.

Older `workflows/my-workflows/GENERATED/NAME/` workflows are migrated to `workflows/my-workflows/NAME/`
when the updated app starts. Their Makefiles, saved paths, and links to past jobs
are updated; no jobs are submitted by this migration.

</blockquote>
</details>

</blockquote>
</details>

<details>
<summary><strong>📄 Workflow files and job specifications</strong></summary>

<blockquote>

A saved workflow is a reusable job definition. The web app creates a folder
directly under `workflows/my-workflows/NAME/`, with files you can inspect or use from the
command line. Solver and problem files remain in their original locations.

```text
workflows/my-workflows/my-workflow/
  Makefile                       # Prepare, build, submit, monitor, sync, stop, clean.
  jobpairs.csv                   # One explicit solver–problem call per row.
  building.txt                   # Runtime roots, optional source roots, recipes, expected outputs.
  resource_limiter_template.txt  # How to wrap each solver command.
  .slurmy-workflow.json           # Saved form choices for editing and duplication.
  configurations.csv             # Present for configuration × problem workflows.
  problem-globs.txt              # Problem files, globs, or ZIP/tar archives for all-pairs workflows.
  axiom-globs.txt                # Optional TPTP include files to package.
  build-resource-0.sh            # Present when build commands were entered in the form.
```

The core runner expects the three specification files. `--batch-size` sets how
many calls each Slurm task owns (default: one); they run one at a time within
that task. Different Slurm tasks can run simultaneously across the cluster.
An optional `--axioms-file axiom-globs.txt` packages TPTP axiom files under a
job-local `Axioms/` and sets `TPTP` to its parent. Otherwise, if present on
the cluster, `TPTP` defaults to `/share/Slurmy-TPTP-v9.2.1`; set
`SLURMY_TPTP_ROOT` before preparing to use a different installation.
On `datalab`, this is a shared, world-readable directory rather than a link
through one user's home directory. Other cluster users can check access with
`test -r /share/Slurmy-TPTP-v9.2.1/Axioms/HWV001-2.ax`.
The Makefile and saved form choices support the web interface; they are not
additional inputs to the core runner. An explicit jobpairs workflow does not
need configurations or globs.

Archive inputs support ZIP and tar files (including gzip, bzip2, and xz tar
variants). Slurmy extracts regular `.p` and `.tptp` files
into an ignored local cache under `workflows/my-workflows/.problem-archives/`, then
packages those files like ordinary problems. Archive entries with unsafe paths
or links are not extracted. Changing an archive creates a new cache entry.

For browser-created workflows, `building.txt` is a JSON list. Each entry has
`root` (local runtime directory), optional `source` (local build context),
`recipe` (Bash script, or empty), and `artifact` (expected path relative to
`root`). The build script receives `$SLURMY_BUILD_WORK` and
`$SLURMY_BUILD_OUTPUT` on the compute node. It must put the declared artifact
under the latter; Slurmy checks for it, then downloads the output directory
into `root`. A blank source is useful when the recipe downloads its own source.
The [solver examples](workflows/example-workflows/README.md) demonstrate local source
directories that are uploaded for each build.
Independent resources build concurrently on separate Slurm build jobs.
Legacy recipes or resources whose source, recipe, and output paths overlap
another build's output run sequentially to preserve their ordering.
Older two-line-per-resource `building.txt` files still work.

A generated Makefile uses the shared targets:

```make
REPO_ROOT := ../../..
BATCH_SIZE := 20
SLURMY_HOST := datalab
SLURMY_PARTITION := CPU-amd
include $(REPO_ROOT)/implementation/templates/workflow.mk
```

For configuration × problem workflows it also regenerates `jobpairs.csv`
from the configurations and globs. Inline build scripts are stored beside the
specifications; existing build scripts can remain elsewhere.

The following examples use existing directories under `/home/me/solvers`
and problems under `/home/me/problems`. Replace these with your own paths.
The web app saves absolute paths; handwritten specification files may use
paths relative to the containing file.
Every CSV row describes exactly one call.

<details>
<summary><strong>📋 jobpairs.csv — explicit calls</strong></summary>

<blockquote>

```csv
solver_name,command,solver_directory,problem,wc_limit,cpu_limit,mem_limit,cores,cpus,exclusive_cpu,exclusive_node
SolverA,./solver --time {{cpu_limit}} {{problem}},/home/me/solvers/solver-a,/home/me/problems/easy.p,70,60,2GiB,1,1,false,false
SolverB,./solver --threads {{cores}} {{problem}},/home/me/solvers/solver-b,/home/me/problems/hard.p,130,120,4GiB,4,1,true,false
SolverA,./solver fixed-input.p,/home/me/solvers/solver-a,/home/me/solvers/solver-a/fixed-input.p,70,60,2GiB,1,1,false,true
```

| Column | Meaning for this single call |
| --- | --- |
| `solver_name` | Optional configuration label shown in calls and solver outcome comparisons. Older CSVs without it use the executable name. |
| `command` | Bash command, with or without placeholders. |
| `solver_directory` | Existing local directory whose cluster copy is the working directory. |
| `problem` | Existing local problem file, also used to identify the call's problem. |
| `wc_limit` | Wall-clock seconds. |
| `cpu_limit` | CPU seconds summed over the call's processes and cores. |
| `mem_limit` | Memory: positive integer with MB, MiB, GB, GiB, TB, or TiB; a bare integer means MB. |
| `cores` | Maximum physical cores the call can execute on; hardware threads are not additional cores. |
| `cpus` | Maximum physical CPU sockets those cores may span, or `auto` to derive placement from the core count on the cluster. With `exclusive_cpu`, enough whole sockets are reserved. The web builder uses `auto`. |
| `exclusive_cpu` | Reserve whole CPU sockets for this call. |
| `exclusive_node` | Reserve an entire node for this call, with no other Slurmy calls in its batch. |

All numeric limits except `cpus=auto` must be positive integers. Exclusivity values are `true`
or `false`, case-insensitive; empty or omitted exclusivity columns mean false.
All other columns are required. Use ordinary CSV quoting for commas, quotes, or
newlines inside a command; CSV does not support comment lines.

Solver-command placeholders: `{{problem}}`, `{{wc_limit}}`,
`{{cpu_limit}}`, `{{mem_limit}}` (decimal MB, rounded up),
`{{mem_limit_mib}}` (MiB, rounded up), and `{{cores}}`. Advanced CSV workflows
with an explicit numeric `cpus` may also use `{{cpus}}`.
Path placeholders are already shell-quoted: leave them bare in the template.
Commands may instead spell everything out; the CSV limits still determine
limiter and Slurm allocations. Ensure hard-coded solver limits agree with them.

</blockquote>
</details>

<details>
<summary><strong>🔨 building.txt — resources and remote recipes</strong></summary>

<blockquote>

The current format is a JSON list. Every resource names its existing local
runtime `root`. A resource with a recipe also names the expected `artifact`
relative to that root. `source` is an optional directory to copy to the build
node; leave it empty when the script downloads its own source.

```json
[
  {
    "root": "/home/me/solvers/solver-a/bin",
    "source": "/home/me/solvers/solver-a/src",
    "recipe": "/home/me/recipes/build-solver-a.sh",
    "artifact": "solver-a"
  },
  {
    "root": "/home/me/solvers/solver-b/bin",
    "source": "",
    "recipe": "",
    "artifact": ""
  },
  {
    "root": "/home/me/solvers/runsolver",
    "source": "",
    "recipe": "/home/me/recipes/build-runsolver.sh",
    "artifact": "runsolver"
  }
]
```

Paths resolve relative to `building.txt`. List the limiter's root here too.
The runtime root may initially be empty, but must exist. A blank recipe means
“package the root as-is.” The older two-line format remains accepted.

Each non-empty recipe runs in a separate Slurm build job, in a disposable copy
of its optional source directory. `SLURMY_BUILD_WORK` points to that copy;
`SLURMY_BUILD_OUTPUT` is a separate empty directory. Put the expected executable
there, preserving the layout expected by your commands. For example:

```bash
#!/usr/bin/env bash
set -euo pipefail
cmake -S "$SLURMY_BUILD_WORK" -B "$SLURMY_BUILD_WORK/build"
cmake --build "$SLURMY_BUILD_WORK/build" --parallel "${SLURM_CPUS_PER_TASK:-1}"
install -m 0755 "$SLURMY_BUILD_WORK/build/solver-a" "$SLURMY_BUILD_OUTPUT/solver-a"
```

The output directory is downloaded back into its declared local root (merging
and replacing matching files), then packaged for the submitted job. Use dedicated
runtime directories: downloaded build outputs may overwrite files there.
Recipes run again on each submission; no build cache is implied.

Build jobs currently use the shared builder's defaults: 4 cores, 4 GiB, and
30 minutes, on the selected partition. These are separate from per-call
limits. For unusual build requirements, use
[the standalone builder](implementation/build/README.md), then leave the
resource's recipe empty.

</blockquote>
</details>

<details>
<summary><strong>⏱️ resource_limiter_template.txt — wrap each command</strong></summary>

<blockquote>

The first word is the limiter executable's local path, relative to this file.
It must lie inside a declared resource root; it may be absent initially if that
root has a build recipe.

```text
/home/me/solvers/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}
```

All solver placeholders are available, plus `{{solver_command}}`,
`{{watcher_log}}`, `{{var_file}}`, and `{{solver_log}}`. Leave these bare too. `{{solver_command}}` is a
shell invocation of the generated solver script, not a quoted command string.

The template must enforce the per-call time and memory limits. A separate
wall-clock watchdog catches a broken limiter after 25 seconds of grace.
Runsolver's optional watcher/variable files provide solver CPU/memory metrics and identify normal time and memory
limits; those outcomes are not infrastructure issues. Other limiters can be
invoked, but their tool-specific exit codes and output formats are not
automatically interpreted as runsolver results.

Slurmy runs the limiter invocation exactly as configured; it does not add
capture options. Include `--watcher-data {{watcher_log}}`, `--var {{var_file}}`,
and `--solver-data {{solver_log}}` to retain those runsolver outputs. The saved
call output separates solver stdout, solver stderr, limiter stdout,
runsolver's watcher log, its measurement variables, and Slurmy controller diagnostics. The controller log is stderr from Slurmy's timeout wrapper and the limiter invocation; it may normally be empty. The job page distinguishes an empty file from a missing file and explains when an output path was omitted from the configured limiter invocation. The job page shows the
rendered limiter command for each call.

</blockquote>
</details>

<details>
<summary><strong>📂 Paths and working directories</strong></summary>

<blockquote>

Relative paths in each input file resolve from that file's directory.
Every solver runs from the cluster copy of its explicit `solver_directory`.
No matching resource root is guessed.

Packaging includes the complete solver directories, every declared resource
root, and each problem file. Their absolute local layout is mirrored under
`~/slurmy/jobs/JOB_ID/rootfs/` on the cluster, so relative references between packaged
directories continue to work. Input symlinks are dereferenced during packaging.

`{{problem}}` and the limiter executable are translated to their cluster paths.
Absolute paths appearing as complete shell words in solver commands are
translated when they refer to packaged resources or listed problems. Paths
embedded inside arguments such as `--file=/local/path`, configuration files,
or shell-generated strings are **not** rewritten. Prefer relative paths inside
your solver directory and `{{problem}}` for the selected input.

A problem's containing directory is not automatically included. TPTP
`include('Axioms/NAME.ax')` searches beneath the `TPTP` environment variable.
By default Slurmy points it at the shared library; adding custom axiom globs
instead copies selected files under a job-local `Axioms/` and points `TPTP`
at its parent. Subpaths below a source `Axioms/` are preserved; other files
are placed by basename, and duplicate destinations are rejected. The two
sources are not combined. For other dependencies,
list their root in `building.txt` with a blank recipe. Package needed scripts, data, shared
libraries, and interpreter environments similarly; a laptop's Python environment
or system libraries are not copied automatically.

Commands and build recipes are trusted executable code. Review them before
submitting. Avoid choosing an unnecessarily broad resource root.

</blockquote>
</details>

<details>
<summary><strong>🔀 Convenience: every configuration × every problem</strong></summary>

<blockquote>

Create `configurations.csv` with the same columns as `jobpairs.csv`, except
`problem`. Then:

```bash
python /path/to/repository/implementation/slurmy-pairs.py \
    --configurations configurations.csv \
    --problems 'problems/easy/*.p' 'problems/hard/**/*.p' \
    --order problem-major \
    --output jobpairs.csv
```

All files from **all** globs are combined and duplicate paths removed.
Each configuration is paired with every selected problem. Quote globs so Python
expands them; an unmatched glob is an error. Repeat `--configurations` for more
configuration tables. CLI globs resolve from your current directory; alternatively
use `--problem-globs-file` with one glob per line, relative to that file.
The output is properly escaped CSV with absolute directory/problem paths.
`--order problem-major` groups calls by problem; `solver-major` groups by
solver configuration and is the default. `random` shuffles all calls
reproducibly for unchanged inputs. The core runner executes an explicit
`jobpairs.csv` in its row order; the web builder applies the selected order
even when you import an explicit CSV.

For arbitrary pairings or per-problem limits, edit or generate `jobpairs.csv`
directly. The core never expands it into additional calls.

</blockquote>
</details>

</blockquote>
</details>

<details>
<summary><strong>🧮 Batching and resource guarantees</strong></summary>

<blockquote>

A **jobpair** is one solver call. A **batch** is one Slurm array task on one node;
it runs its calls one at a time. A **Slurmy job** is the whole submission, which can contain many
Slurm jobs/array tasks.

`--batch-size 20` puts up to 20 calls in each batch, run sequentially.
It does not limit how many batches Slurm runs across the cluster.
Input order is preserved when forming groups; socket-exclusive and ordinary
calls are separated. Node-exclusive calls always get singleton batches.
A call too large for one node is rejected.

For a batch with multiple calls:

| Resource | Slurm reservation |
| --- | --- |
| Wall time | 60 seconds + sum of each call's wall limit + 30 seconds per call, rounded up to minutes. |
| Memory | Largest call memory, rounded up to MiB, plus 128 MiB. |
| Ordinary cores | Largest call core count. |
| Socket-exclusive cores | Enough whole sockets for the largest call core count. |
| Node exclusivity | `--exclusive`, always for a single call. |

The time sum allows each call to use its full limit in sequence.
Slurm has no equivalent per-batch CPU-time cap here: the solver/limiter enforces
each call's CPU limit. Affinity restricts execution to the requested number of
physical cores; Slurmy also sets `OMP_NUM_THREADS`, but solver-specific thread
options remain your responsibility.

Before starting any call, the batch checks its Linux CPU allocation and assigns
disjoint physical cores. Socket-exclusive batches request the full core count
per socket and enough sockets for their calls, then verify complete
sockets, including sibling hardware threads, dedicated to each call.
If Slurm returns an unsuitable or fragmented placement, Slurmy checks before
starting that wave of calls. When this happens in the first wave, it requeues
the same Slurm batch up to five times for a fresh allocation, without weakening
isolation. If placement still fails, the batch ends with a diagnostic. A later
wave is not requeued because earlier calls may already have completed. Requests
that cannot fit the chosen socket/core constraints will not be fixed by a retry.
The current scheduler supports homogeneous partitions only; different
architectures/topologies should use separate submissions.

Exclusivity excludes other scheduled workloads, not operating-system services.
Each batch is currently submitted as a one-element Slurm array, allowing different
resource requests without reserving the largest request for every batch.
Submission keeps at most 32 batches pending or running at once, submitting later
batches as earlier ones finish. Set `SLURMY_MAX_OUTSTANDING` before dispatching to
change that window (for example, `SLURMY_MAX_OUTSTANDING=8 make submit`). If the
cluster's submitted-job limit is already full, Slurmy waits and retries instead
of leaving the remaining calls unsubmitted. The submission command stays active
until every batch has been accepted; the web interface shows build and submission
phases while it runs. A lost connection can still interrupt submission, and the
job page then identifies batches that were never submitted.
`allocations.csv` records the actual batches and reservations after hardware-based
splitting.

</blockquote>
</details>

<details>
<summary><strong>💻 Command-line submission and results</strong></summary>

<blockquote>

The same specifications work without the web app:

```bash
python /path/to/repository/implementation/slurmy.py jobpairs.csv building.txt resource_limiter_template.txt --batch-size 20
# Up to 20 calls per Slurm task, running one at a time in each task.
bash ./submit.sh  # Build remotely, fetch artifacts, package inputs, and submit.
```

Generation does not connect to the cluster. It writes `submit.sh` and
`submit.sh.files/` beside `jobpairs.csv` and refuses to overwrite them.
Workflow Makefiles handle regeneration automatically when inputs or templates
change. Editing `BATCH_SIZE` in the workflow Makefile refreshes the
scripts on the next `make`. For a one-off command-line override,
run `make clean` first because an override alone does not change file timestamps.

The shared Makefile supports `make` (prepare), `make build`, `make prepare-submit`
(prepare and dispatch), `make submit` (dispatch previously prepared files only),
`make monitor`, `make sync`, `make stop`, and `make clean`.
`make clean` removes submission scripts/helpers, retaining specification files
and downloaded binaries. Submission builds resources again, even if you ran
`make build` first.

```bash
python implementation/slurmy-sync.py --follow        # Incrementally sync the newest job.
python implementation/slurmy-sync.py JOB_ID          # Sync one job once.
python implementation/slurmy-cancel.py               # Choose from active Slurm jobs.
python implementation/slurmy-cancel.py SLURM_JOB_ID  # Cancel this Slurm ID non-interactively.
```

Sync uses rsync to avoid downloading unchanged archives. Results and refreshed
metadata are saved under `job-results/JOB_ID/`; see `python implementation/slurmy-sync.py --help`
for destination and refresh options. `make sync SLURMY_ID=JOB_ID` selects a
particular Slurmy job instead of the latest one.

```text
submit.sh                         # Local entry point: build, package, transfer, submit.
submit.sh.files/
  remote_prepare.sh               # Creates the remote job directory.
  remote_submit.sh                # Checks topology, plans allocations, calls sbatch.
  batch.sh                        # Checks physical-core placement; launches calls.
  call.sh                         # Runs the limiter; captures and publishes results.
  timed_call.sh                   # Measures elapsed/CPU time.
  csv.sh                          # CSV escaping shared by remote helpers.
  calls/                          # Per-call solver/limiter scripts and configuration.
  plans/                          # Proposed batches before hardware-based splitting.
  builds/                         # Build driver and generated recipe wrappers.
  archive-paths.txt                # Local paths to package.
  metadata.json                   # Counts, format version, and scheduling policy.
  manifest.jsonl                  # Full per-call definitions.
```

Remote jobs live under `~/slurmy/jobs/NAME_TIMESTAMP_PID/`, where the prefix comes
from the chosen job name or, by default, the workflow folder name (safely
shortened if needed). Slurm batch names use that prefix; build jobs use the
workflow name. Existing username-prefixed jobs remain readable.
Builds use `~/slurmy/builds/`. Remote job directories also contain `batches/`,
`allocations.csv`, `submission.csv`, `submission.state`, `progress/`, `logs/`, and `results/`.
Slurmy creates only `~/slurmy/` at the cluster-home top level; it keeps jobs
and builds in its `jobs/` and `builds/` children. Those three directories have
mode `700`, so only their owner can traverse them.
Each call publishes a CSV result and compressed output archive independently,
so completed calls can be synced while other calls continue.

Submission is not transactional: if a later sbatch fails, earlier accepted
batches remain submitted and can be cancelled. Historical results remain
readable. The three-file interface replaces the earlier `.solver` interface;
regenerate old submission scripts before using them.

</blockquote>
</details>
