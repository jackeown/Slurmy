<div align="center">
  <img src="logo.svg" alt="Slurmy logo" width="240">

# Slurmy

</div>

Run resource-limited theorem-prover experiments on a Slurm cluster. Specify
each solver–problem call explicitly, or generate all combinations as a convenience.
Build on the cluster, inspect the generated shell scripts, monitor progress,
and synchronize results.

1. Prepare `jobpairs.csv`, `building.txt`, and `resource_limiter_template.txt`.
2. Run `slurmy.py` to generate a readable `submit.sh` and its helper files.
3. Run `submit.sh`: build dependencies on compute nodes, download them for
   packaging, transfer the experiment, and submit its batches.
4. Open the local web app to monitor, inspect output, and manage the experiment.

Python runs on your computer. The cluster runs Bash, Slurm commands, and your
cluster-built executables. See [ExampleRuns](ExampleRuns/README.md) for ready-made
Vampire, E, and Drodi workflows, or use the
[web experiment builder](YourRuns/example-generator/README.md).

```bash
python -m pip install -r requirements.txt
python slurmy-web.py  # Starts the local app if needed and opens your browser.
```

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
<summary><strong>📝 Create your own submission</strong></summary>

<blockquote>

The core interface does not infer solver–problem combinations or choose a
working directory from a build recipe. Every CSV row describes exactly one call.

```bash
python /path/to/Slurmy/slurmy.py jobpairs.csv building.txt resource_limiter_template.txt --deg_par 4
# --deg_par: maximum number of jobpairs running simultaneously within EACH batch.

bash ./submit.sh  # Build remotely, fetch artifacts, package inputs, and submit.
```

Generated files appear beside `jobpairs.csv`. Generation does not connect to
the cluster. Existing submission files are not overwritten.

<details>
<summary><strong>📋 jobpairs.csv — explicit calls</strong></summary>

<blockquote>

```csv
command,solver_directory,problem,wc_limit,cpu_limit,mem_limit,cores,cpus,exclusive_cpu,exclusive_node
./solver --time {{cpu_limit}} {{problem}},resources/solver-a,problems/easy.p,70,60,2GiB,1,1,false,false
./solver --threads {{cores}} {{problem}},resources/solver-b,problems/hard.p,130,120,4GiB,4,1,true,false
./solver fixed-input.p,resources/solver-a,resources/solver-a/fixed-input.p,70,60,2GiB,1,1,false,true
```

| Column | Meaning for this single call |
| --- | --- |
| `command` | Bash command, with or without placeholders. |
| `solver_directory` | Existing local directory whose cluster copy is the working directory. |
| `problem` | Existing local problem file, also used to identify the benchmark. |
| `wc_limit` | Wall-clock seconds. |
| `cpu_limit` | CPU seconds summed over the call's processes and cores. |
| `mem_limit` | Memory: positive integer with MB, MiB, GB, GiB, TB, or TiB; a bare integer means MB. |
| `cores` | Maximum physical cores the call can execute on; hardware threads are not additional cores. |
| `cpus` | Maximum physical CPU sockets those cores may span. With `exclusive_cpu`, this many whole sockets are reserved. |
| `exclusive_cpu` | Reserve whole CPU sockets for this call. |
| `exclusive_node` | Reserve an entire node for this call, with no other Slurmy calls in its batch. |

All numeric limits must be positive integers. Exclusivity values are `true`
or `false`, case-insensitive; empty or omitted exclusivity columns mean false.
All other columns are required. Use ordinary CSV quoting for commas, quotes, or
newlines inside a command; CSV does not support comment lines.

Solver-command placeholders: `{{problem}}`, `{{wc_limit}}`,
`{{cpu_limit}}`, `{{mem_limit}}` (decimal MB, rounded up),
`{{mem_limit_mib}}` (MiB, rounded up), `{{cores}}`, and `{{cpus}}`.
Path placeholders are already shell-quoted: leave them bare in the template.
Commands may instead spell everything out; the CSV limits still determine
limiter and Slurm allocations. Ensure hard-coded solver limits agree with them.

</blockquote>
</details>

<details>
<summary><strong>🔨 building.txt — resources and remote recipes</strong></summary>

<blockquote>

Each resource takes exactly two lines: its existing local root directory,
then a Bash build-script path. A blank second line means “package as-is.”

```text
resources/solver-a
recipes/build-solver-a.sh
resources/solver-b

resources/runsolver
recipes/build-runsolver.sh
```

Paths resolve relative to `building.txt`. List the limiter's root here too.
Keep the blank recipe line even for the final resource. Empty resource roots
are allowed when a recipe clones the sources remotely.

Each non-empty recipe runs in a separate Slurm build job, in a disposable copy
of its resource root. `SLURMY_BUILD_WORK` points to that copy;
`SLURMY_BUILD_OUTPUT` points to the same directory for these generated recipes.
Build in place or install outputs there, preserving the layout expected by your
commands. For example:

```bash
#!/usr/bin/env bash
set -euo pipefail
cmake -S "$SLURMY_BUILD_WORK" -B "$SLURMY_BUILD_WORK/build"
cmake --build "$SLURMY_BUILD_WORK/build" --parallel "${SLURM_CPUS_PER_TASK:-1}"
```

The resulting tree is downloaded back into its declared local root (merging
and replacing matching files), then packaged for the experiment. Use dedicated
resource directories: downloaded build outputs may overwrite files there.
Recipes run again on each submission; no build cache is implied.

Build jobs currently use the shared builder's defaults: 4 cores, 4 GiB, and
30 minutes, on the selected partition. These are separate from experiment
limits. For unusual build requirements, use
[the standalone builder](building-dependencies/README.md), then leave the
resource's recipe line blank.

</blockquote>
</details>

<details>
<summary><strong>⏱️ resource_limiter_template.txt — wrap each command</strong></summary>

<blockquote>

The first word is the limiter executable's local path, relative to this file.
It must lie inside a declared resource root; it may be absent initially if that
root has a build recipe.

```text
resources/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}
```

All solver placeholders are available, plus `{{solver_command}}`,
`{{watcher_log}}`, `{{var_file}}`, `{{solver_log}}`, and
`{{controller_log}}`. Leave these bare too. `{{solver_command}}` is a
shell invocation of the generated solver script, not a quoted command string.

The template must enforce the per-call time and memory limits. A separate
wall-clock watchdog catches a broken limiter after 25 seconds of grace.
Runsolver's optional watcher/variable files provide solver CPU/memory metrics and identify normal time and memory
limits; those outcomes are not infrastructure issues. Other limiters can be
invoked, but their tool-specific exit codes and output formats are not
automatically interpreted as runsolver results.

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
`~/Slurmy/JOB_ID/rootfs/` on the cluster, so relative references between packaged
directories continue to work. Input symlinks are dereferenced during packaging.

`{{problem}}` and the limiter executable are translated to their cluster paths.
Absolute paths appearing as complete shell words in solver commands are
translated when they refer to packaged resources or listed problems. Paths
embedded inside arguments such as `--file=/local/path`, configuration files,
or shell-generated strings are **not** rewritten. Prefer relative paths inside
your solver directory and `{{problem}}` for the selected input.

A problem's containing directory is not automatically included. If a benchmark
includes other files (e.g. TPTP axioms), list their shared root in
`building.txt` with a blank recipe. Package needed scripts, data, shared
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
python /path/to/Slurmy/slurmy-pairs.py \
    --configurations configurations.csv \
    --problems 'problems/easy/*.p' 'problems/hard/**/*.p' \
    --output jobpairs.csv
```

All files from **all** globs are combined and duplicate paths removed.
Each configuration is paired with every selected problem. Quote globs so Python
expands them; an unmatched glob is an error. Repeat `--configurations` for more
configuration tables. CLI globs resolve from your current directory; alternatively
use `--problem-globs-file` with one glob per line, relative to that file.
The output is properly escaped CSV with absolute directory/problem paths.

For arbitrary pairings or per-problem limits, edit or generate `jobpairs.csv`
directly. The core never expands it into additional calls.

</blockquote>
</details>

</blockquote>
</details>

<details>
<summary><strong>🧮 Batching and resource guarantees</strong></summary>

<blockquote>

A **jobpair** is one solver call. A **batch** is one concurrent wave of calls
on one node. A **Slurmy job** is the whole submission, which can contain many
Slurm jobs/array tasks.

`--deg_par 4` permits at most four calls simultaneously **in each batch**.
It does not limit how many batches Slurm runs across the cluster.
Input order is preserved when forming groups; socket-exclusive and ordinary
calls are separated. Node-exclusive calls always get singleton batches.
Groups are split further if the partition's cores, sockets, or memory cannot
hold them; a call too large for one node is rejected.

For a batch containing P calls:

| Resource | Slurm reservation |
| --- | --- |
| Wall time | 60 seconds + sum of each call's wall limit + 30 seconds per call, rounded up to minutes. |
| Memory | P × (largest call memory, rounded up to MiB + 128 MiB). |
| Ordinary cores | P × largest call core count. |
| Socket-exclusive cores | P × largest call socket count × physical cores per socket. |
| Node exclusivity | `--exclusive`, always for a single call. |

The time sum is deliberately conservative even though calls execute concurrently.
Slurm has no equivalent per-batch CPU-time cap here: the solver/limiter enforces
each call's CPU limit. Affinity restricts execution to the requested number of
physical cores; Slurmy also sets `OMP_NUM_THREADS`, but solver-specific thread
options remain your responsibility.

Before starting any call, the batch checks its Linux CPU allocation and assigns
disjoint physical cores. Socket-exclusive batches request the full core count
per socket and enough sockets for their calls, then verify complete
sockets, including sibling hardware threads, dedicated to each call.
If Slurm returns an unsuitable or fragmented placement, the batch fails with
a diagnostic **before running any solver**. It does not silently weaken isolation
or automatically retry. The current scheduler supports homogeneous partitions
only; different architectures/topologies should use separate submissions.

Exclusivity excludes other scheduled workloads, not operating-system services.
Each batch is currently submitted as a one-element Slurm array, allowing different
resource requests without reserving the largest request for every batch.
`allocations.csv` records the actual batches and reservations after hardware-based
splitting.

</blockquote>
</details>

<details>
<summary><strong>🌐 Local web app: create, monitor, and manage experiments</strong></summary>

<blockquote>

```bash
python slurmy-web.py                  # Open job history; reuse an existing server.
python slurmy-web.py --new            # Open the guided experiment builder.
python slurmy-web.py --directory ExampleRuns/example-vampire
python slurmy-web.py --job JOB_ID     # Open a particular submitted experiment.
python slurmy-web.py --stop           # Stop only the local app, not Slurm jobs.
```

`make monitor` in an example opens that experiment's page. In
`YourRuns/example-generator`, both `make` and `make monitor` open the builder.
Repeated invocations reuse the same server. New submissions record their local
workflow directory so their jobs appear on the corresponding experiment page;
older jobs without this field remain in the full job history.

The app has job history, searchable/paginated calls, saved solver output,
Slurm allocations and cancellation, scheduler logs, and local experiment pages.
The guided builder supports explicit jobpair CSVs, imported configuration CSVs,
or interactive solver configurations crossed with problem globs; optional
remote build scripts/commands and limiter templates are included. Path fields
are checked when you leave them. Preview validates the complete specification
before saving it under `YourRuns/GENERATED`.

Saving creates files only. **Prepare scripts** runs `make all`; **Build & submit**
runs `make submit` after confirmation, with an operation log in the page.
Only trusted local Makefiles/commands should be run. A job's **Sync results**
button downloads current results once; **Cancel** confirms the selected active
Slurm job ID. The terminal tools are still available for scripting:

```bash
python slurmy-sync.py --follow        # Incrementally sync the newest job.
python slurmy-sync.py JOB_ID          # Sync a particular job once.
python slurmy-cancel.py               # Choose from active jobs.
python slurmy-cancel.py SLURM_JOB_ID   # Cancel this Slurm ID non-interactively.
```

The web monitor shows per-call progress and elapsed times; select a completed call to
inspect archived solver, watcher, and controller output. Percent complete counts
finished calls, including ordinary resource-limit outcomes—it is not a prediction
of how far a running proof search has progressed. Solver output can establish
whether a completed call actually proved the benchmark.

The app uses Flask with Waitress and listens **only on 127.0.0.1**, normally
port 8765. It has no accounts or multi-user isolation: do not expose it through
a proxy, public tunnel, or shared server. It can read local paths and run trusted
workflow commands with your permissions. Cross-site requests and unprotected
state-changing requests are rejected. No JavaScript build tools, CDN, or database
are required; Textual/Rich are no longer dependencies.

`--host` / `SLURMY_HOST` selects the SSH alias, not the web bind address.
Use `--port` / `SLURMY_WEB_PORT` for a different local port, `--no-browser` to
print the URL, or `--serve` to run in the foreground. The server stays running
after a browser tab closes. Restart it after updating the code or dependencies.
Server and operation logs are stored in the ignored `.slurmy-web/` directory;
operation tracking is in memory, so let builds/submissions finish before stopping
the app. `--stop` refuses while a web-launched operation is running.

On Windows, run the app and tools inside WSL and open the printed localhost URL
in your Windows browser if automatic opening is unavailable. The app must run on
the machine whose local file paths you enter. The legacy `slurmy-monitor.py` and
generator script now open the same web app instead of separate terminal UIs.

Sync uses rsync to avoid downloading unchanged archives. It writes local results
and regularly refreshed metadata under `slurmy-results/JOB_ID/`; see
`slurmy-sync.py --help` for destination and refresh options. Existing historical
results remain readable, including older formats.

Remote experiments live under `~/Slurmy/USER_TIMESTAMP_PID/`; remote builds
use `~/Slurmy-builds/`. Failures remain available for inspection. Submission is
not transactional: if a later sbatch fails, earlier accepted batches remain
submitted and can be cancelled with the cancellation tool.

```text
submit.sh                         # Local entry point: build, package, transfer, submit.
submit.sh.files/
  remote_prepare.sh               # Creates this experiment's remote directory.
  remote_submit.sh                # Checks topology, plans allocations, calls sbatch.
  batch.sh                        # Checks physical-core placement; launches calls.
  call.sh                         # Runs the limiter; captures and publishes results.
  timed_call.sh                   # Measures elapsed/CPU time using Bash's timer.
  csv.sh                          # CSV escaping shared by remote helpers.
  calls/                          # Per-call solver/limiter scripts and configuration.
  plans/                          # Proposed batches before hardware-based splitting.
  builds/                         # Shared build driver and generated recipe wrappers.
  archive-paths.txt                # NUL-delimited local paths to package.
  metadata.json                   # Counts, format version, and scheduling policy.
  manifest.jsonl                  # Full per-call definitions for inspection/monitoring.
```

On the cluster, `batches/`, `allocations.csv`, `submission.csv`,
`progress/`, `logs/`, and `results/` are added. Each call publishes a CSV result
and a compressed output archive independently, so completed calls can be synced
while other calls are still running.

This three-file interface replaces the earlier `.solver`/many-flags interface.
Regenerate old submission scripts; do not submit them expecting the new behavior.
In example workflows, `make clean` removes only generated submission files.

</blockquote>
</details>
