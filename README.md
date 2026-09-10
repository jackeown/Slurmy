<div align="center">
  <img src="logo.svg" alt="Slurmy logo" width="240"><br>

# Slurmy

</div>

Run reproducible, resource-limited theorem-prover experiments on a Slurm
cluster.

Slurmy turns local solver descriptions and problem files into an inspectable
Slurm job-array submission. It copies everything the experiment needs to the
cluster, measures and limits each solver call with `runsolver`, and provides
separate tools for live monitoring and incremental result synchronization.


## Usage

1. Describe one or more solver command lines and select the problem files.
2. Run `slurmy.py` locally to generate `submit.sh` and `submit.sh.files/`.
3. Inspect and run `submit.sh` to copy the experiment and submit a Slurm array.
4. Follow the job with `slurmy-monitor.py` or download results with
   `slurmy-sync.py`.

Python runs only on the local machine. Compute nodes run the generated Bash
scripts, Slurm commands, and the included `runsolver` binary directly.


---

<details>
<summary><strong>🚀 Quick start</strong></summary>

<blockquote>

<details>
<summary><strong>✅ Requirements and dependencies</strong></summary>

On the local machine:

- Python 3.9+, Bash, `make`, `ssh`, `scp`, `rsync`, and tar (GNU or BSD)
- Non-interactive SSH access to the cluster

Use Linux, macOS (Intel or Apple Silicon, including M4), or Windows through
WSL. The laptop and cluster can have different CPU architectures: the example
Makefiles build provers and `runsolver` in Slurm jobs, download them for
packaging, and execute them only on the cluster. Custom solver installations
must likewise be built for the cluster.

**macOS:** with [Homebrew](https://brew.sh/) installed, run:

```bash
brew install python rsync
```

If `make` is missing, install Apple's command-line tools with
`xcode-select --install`. Enter `bash` in Terminal before following the shell
examples below; this also allows pasting their comments into macOS Terminal.

The built-in Bash and tar are sufficient; no GNU tar installation is needed.
Use Homebrew's Python to create the environment below.

**Linux (Ubuntu/Debian):** install any missing prerequisites:

```bash
sudo apt install python3 python3-venv make openssh-client rsync tar git
```

**Windows:** install [WSL with Ubuntu](https://learn.microsoft.com/en-us/windows/wsl/install)
using `wsl --install` in an administrator PowerShell, then follow the Linux
setup inside Ubuntu. Run all Slurmy commands there, keep the checkout under
your Linux home directory (such as `~/Slurmy`), and configure SSH inside WSL.
Native PowerShell and Windows Python are not supported.

On the cluster:

- Slurm, Bash, `rsync`, GNU `tar`, `awk`, `sed`, and `base64`

Building all three included prover examples additionally requires `git`,
`curl`, `sha256sum`, `make`, C and C++ compilers, and CMake 3.14 or newer on
the cluster compute nodes.

`datalab` is the default SSH host. It can be an alias in `~/.ssh/config`:

```sshconfig
Host datalab
    HostName cluster.datalab.tuwien.ac.at
    User your-account
    IdentityFile ~/.ssh/your-key
```

The submission generator, build, monitor, and sync scripts accept `--host HOST`.
You can instead set `SLURMY_HOST` once for the shell; an explicit `--host`
always wins:

```bash
export SLURMY_HOST=another-cluster
```

Connect to the cluster VPN if needed, then verify access with
`ssh datalab 'command -v sbatch'` (substitute your host alias). Complete any
first-connection host-key and authentication setup before running the examples.
The interactive example generator asks for the host explicitly.

`slurmy.py`, `building-dependencies/slurmy-build.py`, and `slurmy-sync.py` use
only the Python standard library. The `slurmy-monitor.py` dashboard and
interactive example generator additionally need Textual, which is installed
through `requirements.txt`:

```bash
# Optional if your existing environment already provides a suitable `python`.
python3 -m venv .venv
source .venv/bin/activate

python -m pip install -r requirements.txt
```

The Makefiles use `python` from your PATH. Activating the environment above
provides that command on macOS and Linux without changing the system Python.

</details>

<details>
<summary><strong>🧭 Generate your own workflow interactively</strong></summary>

Run the example generator when you want a guided alternative to writing a
Makefile and solver description by hand:

```bash
cd examples/example-generator
make
```

It asks one question at a time with no default answers. It can create a remote
build recipe like the Vampire and E examples, use existing solver-description
files, or create a description for an existing solver root. For a remote build,
you enter an optional source directory, the Bash build steps, the executable
produced below `$SLURMY_BUILD_OUTPUT`, its invocation, and the Slurm resources
for compilation. Solver, source, and problem paths are validated as they are
entered. Relative paths start from `examples/example-generator/` and are
converted to absolute paths in the generated workflow.

The result is written to `examples/GENERATED/example-NAME/`. A remote-build
workflow downloads its cluster-built executable into that directory's `bin/`;
a workflow using existing inputs keeps absolute references to them. The normal
submission step packages the resulting solver and selected benchmarks. Review
the generated settings, then use the standard targets:

```bash
cd ../GENERATED/example-NAME
make
make submit
make monitor
```

See the [example-generator documentation](examples/example-generator/README.md)
for the exact validation and path behavior.

</details>

<details>
<summary><strong>🧪 Run an included example</strong></summary>

The Vampire example provides the shortest complete path from source code to a
submitted experiment:

```bash
cd examples/example-vampire
make
make submit
make monitor
```

`make` submits Slurm build jobs for Vampire and `runsolver`, downloads their
cluster-built binaries, then generates the submission files. `make submit`
transfers them to `datalab` and returns once Slurm accepts the array. `make
monitor` opens the interactive dashboard.

See [Prover examples](examples/README.md) for the E, Drodi, and combined
three-prover examples, the reusable workflow template, the interactive example
generator, and the available Makefile targets.

</details>

</blockquote>

</details>

<details>
<summary><strong>🛠️ Create your own submission</strong></summary>

<blockquote>

<details>
<summary><strong>1. 🔧 Build runsolver on the cluster</strong></summary>

Run the runsolver recipe as a Slurm job, validate its output, and download the
binary for inclusion in generated submission files:

```bash
python ./building-dependencies/slurmy-build.py \
  --name runsolver \
  --recipe ./building-dependencies/runsolver/build.sh \
  --output ./building-dependencies/runsolver \
  --artifact runsolver \
  --sbatch-option=--partition=CPU-amd
```

This creates `building-dependencies/runsolver/runsolver` using the compute-node
architecture. Slurmy copies that binary to `submit.sh.files/runsolver`. The
included example Makefiles run this build automatically.

</details>

<details>
<summary><strong>2. 📝 Describe the solver</strong></summary>

Create `solver.solver`. The comments shown here are valid description-file
comments and are ignored by Slurmy:

```text
./solver-root
# The first line above is the directory to package. Relative paths start from
# the directory containing this description file. Commands run from the
# packaged copy of that directory on the cluster.
#
# Every later non-empty, non-comment line is a complete solver configuration.
# Supported variables in those command lines are:
#   {{problem}}                    one call for every problem selected by every --problems glob
#   {{problem=benchmarks/test.p}}  one call for only this file; paths are relative to solver.solver
#   {{cpu-limit}}                  CPU limit in seconds
#   {{wc-limit}}                   wall-clock limit in seconds
#   {{mem-limit}}                  memory limit in decimal megabytes
#
# This configuration runs once for every selected problem:
./bin/my-solver --strategy default --cpu-limit {{cpu-limit}} --wall-limit {{wc-limit}} --memory-limit-mb {{mem-limit}} {{problem}}
# A second configuration produces another call for every selected problem:
./bin/my-solver --strategy fallback --cpu-limit {{cpu-limit}} {{problem}}
# A specific problem produces only this one call and does not expand the globs:
./bin/my-solver --check-only {{problem=benchmarks/smoke-test.p}}
#
# Each command must use either generic or specific problem variables, not both.
# Commands are trusted shell input, so use only descriptions you trust.
# Use one description per solver root and repeat --solver to add more solvers.
```

</details>

<details>
<summary><strong>3. ⚙️ Generate the submission files</strong></summary>

```bash
slurmy_args=(
  --host "${SLURMY_HOST:-datalab}"                # SSH host used to submit and later inspect the experiment.
  --cpu-limit 60                                 # Maximum CPU seconds for each solver call.
  --wc-limit 70                                  # Maximum elapsed seconds for each solver call.
  --mem-limit 2GiB                               # Memory limit enforced on each solver call.
  --cpu-request 1-core                           # Slurm CPUs requested for each array element.
  --memory-request 2300MiB                       # Slurm memory request; leave room above mem-limit.
  --problems 'problems/easy/**/*.p'              # Add every file matched by this quoted glob.
  --problems 'problems/hard/**/*.p'              # Add these matches too; duplicate paths are removed.
  --solver solver.solver                         # Solver root and command-line configurations.
  --runsolver building-dependencies/runsolver/runsolver  # Cluster-built runsolver binary to package.
  --batch-size 20                                # Solver calls run sequentially in each array element.
  --max-parallel 100                             # Maximum number of Slurm array elements allowed to run simultaneously.
  --sbatch-option=--partition=CPU-amd            # Tell Slurm to use machines in the CPU-amd partition.
  --output submit.sh                             # Creates submit.sh and submit.sh.files/.
)
./slurmy.py "${slurmy_args[@]}"                      # Generate files locally; do not submit yet.
```

All `--problems` occurrences contribute to one problem set. Every glob must
match at least one regular file, and files matched more than once appear only
once. Generating the submission does not contact the cluster. It creates:

```text
submit.sh                         # Laptop script that packages, copies, and submits the job.
submit.sh.files/                  # Companion directory required by submit.sh.
  archive-paths.txt               # Laptop solver and problem paths to package.
  metadata.json                   # Limits, requests, task counts, and result columns.
  remote_prepare.sh               # Creates the remote job directory over SSH.
  remote_submit.sh                # Extracts uploaded files and invokes sbatch over SSH.
  runsolver                       # Local runsolver binary copied to the cluster.
  slurm_job.sh                    # Slurm compute-node script that runs each batch.
  batches/                        # Generated task data, divided by array element.
    batch_000000.sh               # Solver commands for the first array element.
    batch_000001.sh               # Solver commands for the second array element.
```

All generated scripts and task commands are readable. There are no encoded
payloads or remote programs hidden inside SSH command strings. Keep
`submit.sh` and `submit.sh.files/` together.

The generated `submit.sh` documents its fixed configuration, companion files,
and each packaging and submission stage directly in comments.

</details>

<details>
<summary><strong>4. 🚀 Inspect and submit</strong></summary>

The most useful files to inspect are:

```bash
less submit.sh
less submit.sh.files/slurm_job.sh
less submit.sh.files/batches/batch_000000.sh
less submit.sh.files/metadata.json
```

Submit with:

```bash
./submit.sh
```

The script prints the Slurm job ID and remote directory. It submits the work
but does not wait for it to finish.

```text
Slurmy job ID: alice_1786464000_12345
Slurm job ID(s): 123456
Remote directory: /home/alice/Slurmy/alice_1786464000_12345
```

A Slurmy ID contains the remote username, submission time in Unix seconds, and
the laptop submission script's process ID. The process ID keeps simultaneous
submissions made during the same second from choosing the same remote path.

Monitor it normally with Slurm:

```bash
ssh datalab squeue -j 123456
ssh datalab sacct -j 123456
```

</details>

</blockquote>

</details>

<details>
<summary><strong>📊 Monitor and retrieve jobs</strong></summary>

<blockquote>

<details>
<summary><strong>👀 Monitor jobs</strong></summary>

Open the interactive dashboard on your laptop:

```bash
./slurmy-monitor.py
```

It discovers every current and past Slurmy job under `$HOME/Slurmy/` on `datalab`
and refreshes automatically. The six tabs provide:

- **Jobs:** all submissions, their state, completion percentage, task counts,
  execution errors, elapsed time, and submission time;
- **Tasks:** every solver call, including its system, benchmark, live state,
  wall time, CPU time, peak memory, and exit code;
- **Output:** the selected task's combined solver stdout/stderr, runsolver
  controller output, watcher report, and recorded variables;
- **Slurm:** live array placement and pending reasons together with historical
  accounting records;
- **Logs:** the latest portion of each Slurm array log;
- **Details:** limits, resource requests, status totals, remote location, and
  the complete job metadata.

Select a row with the arrow keys or mouse and press Enter to open its tasks.
On a saved task, press Enter or `O` to inspect its output. Use `1` through `6`
to change tabs, `R` to refresh immediately, and `Q` to quit. Use another SSH
host or a slower refresh interval when needed:

```bash
./slurmy-monitor.py --host another-cluster --refresh 15
```

The system name is inferred from the executable in the solver command—for
example, `./vampire` is shown as `vampire`. Time and memory values automatically
choose readable units, such as milliseconds for short calls and bytes rather
than `0 KiB` for values smaller than one KiB.

The dashboard is read-only. It runs `ssh`, `squeue`, and `sacct`, and reads
small metadata, progress, result-summary, and log files. It does not transfer
solver inputs or whole result archives. When you open a task's output, the
needed files are extracted from its archive on the cluster and only those files
are sent to the dashboard. Each displayed file is limited to 1 MiB; for a
larger file, the first and last 512 KiB are shown. A job remains in the
dashboard as long as its remote `$HOME/Slurmy/<job-id>/` directory remains
available.

`time-limit` and `memory-limit` are normal solver results and are not counted as
execution errors. An execution error means that Slurmy could not run or record
work normally—for example, a solver launch error, corrupt task data, an interrupted
worker, a failed Slurm task, or a node failure. Select an affected task and
press `O` to inspect its output. Scheduler-level errors are shown in the
**Slurm** and **Logs** tabs.

The dashboard distinguishes a solver `time-limit` from Slurm's `TIMEOUT`
state. The former is expected; the latter means Slurm stopped an array task
before Slurmy saved all of its results, so it is an execution error.

</details>

<details>
<summary><strong>📥 Sync results</strong></summary>

Download the latest Slurmy job from `datalab`:

```bash
./slurmy-sync.py
```

The command performs one incremental sync and exits. Unchanged files are not
transferred again. By default, files are stored under
`slurmy-results/<Slurmy-ID>/`. Supply an ID to sync a particular job or choose an
exact local directory:

```bash
./slurmy-sync.py john.keown_1787570882
./slurmy-sync.py john.keown_1787570882 --output ~/results/vampire-run
```

Use follow mode while a job is running:

```bash
./slurmy-sync.py john.keown_1787570882 --follow --interval 5
```

Each pass downloads only the remote job's metadata and batch definitions,
progress records, Slurm logs, result summaries, and finalized result archives.
Solver inputs and executable runtime files are not downloaded. Follow mode
stops once every planned solver call has a complete result; pressing Ctrl-C
earlier keeps everything already downloaded.

Every pass atomically replaces `sync-metadata.json` in the local job directory.
It reports completion and solving separately, along with execution-status and
SZS-status counts, aggregate CPU/wall/memory measurements, archive counts,
bytes downloaded, and the time of the latest sync. For example:

```bash
watch -n 1 cat slurmy-results/john.keown_1787570882/sync-metadata.json
```

`percent_complete` measures calls with final runsolver records.
`percent_solved` measures calls whose archived solver output contains a solved
SZS status such as `Theorem`, `Unsatisfiable`, `Satisfiable`, or
`CounterSatisfiable`. A solver without SZS status lines can still be 100%
complete while reporting 0% solved.

</details>

</blockquote>

</details>

<details>
<summary><strong>🧠 Advanced usage</strong></summary>

<blockquote>

<details>
<summary><strong>🏗️ Building software on the cluster</strong></summary>

`building-dependencies/slurmy-build.py` is the shared remote-build mechanism
used for runsolver, Vampire, E, and Drodi. It copies a Bash recipe to the SSH
host, submits the recipe with `sbatch`, waits for its final state, checks every
declared artifact, and uses `rsync` to download the output. Compilation never
runs on the SSH head node or on the laptop.

A recipe runs on one compute node with these directories available:

```bash
$SLURMY_BUILD_ROOT    # Persistent directory containing the recipe and build log.
$SLURMY_BUILD_WORK    # Empty working directory for sources and intermediate files.
$SLURMY_BUILD_OUTPUT  # Put every artifact that should be downloaded here.
```

For example, a recipe that builds a local source tree can be submitted with a
build context:

```bash
python ./building-dependencies/slurmy-build.py \
  --host "${SLURMY_HOST:-datalab}" \
  --name my-solver \
  --recipe ./build-my-solver.sh \
  --context ./my-solver-source \
  --output ./solver-root/bin \
  --artifact my-solver \
  --cpus-per-task 8 \
  --memory 8GiB \
  --time 00:30:00 \
  --sbatch-option=--partition=CPU-amd
```

The optional context is unpacked into `$SLURMY_BUILD_WORK`. A recipe may also
clone or download its source directly, as the included recipes do. Remote build
directories and logs remain under `$HOME/Slurmy-builds/` for diagnosis.

</details>

<details>
<summary><strong>📁 Structuring larger submissions</strong></summary>

Treat the solver root named on the first line of a solver description as a
self-contained directory. Put every solver-side file needed on the cluster
under that root: executables, scripts, libraries, configuration files, and
other data. For example:

```text
experiment/
  solver.solver
  solver-root/
    bin/my-solver
    configs/competition.toml
    libraries/
    problems/
      axioms/
      batch-a/problem.p
```

The corresponding `experiment/solver.solver` can use paths relative to the
solver root:

```text
./solver-root
./bin/my-solver --config configs/competition.toml {{problem}}
```

The first line is resolved relative to the solver description file, and that
whole directory is packaged recursively. On each compute node, Slurmy changes to
the packaged copy of that directory before running the command. Consequently,
relative command paths such as `./bin/my-solver`, `configs/competition.toml`,
and `libraries/` continue to work without alteration.

Slurmy preserves each input's absolute laptop path underneath a private `rootfs`
inside the remote job directory. For a job named `alice_1786464000_12345`, the
mapping looks like this:

| Laptop path | Path used on the cluster |
| --- | --- |
| `/home/alice/work/experiment/solver-root` | `$HOME/Slurmy/alice_1786464000_12345/rootfs/home/alice/work/experiment/solver-root` |
| `/home/alice/work/experiment/solver-root/problems/batch-a/problem.p` | `$HOME/Slurmy/alice_1786464000_12345/rootfs/home/alice/work/experiment/solver-root/problems/batch-a/problem.p` |

Problem globs passed to `--problems` are resolved from the directory in which
`slurmy.py` is run. A path in `{{problem=/path/to/problem}}` is instead resolved
relative to the solver description file when it is not absolute. Selected
problem files are packaged, and every problem placeholder in a generated task
is replaced with its full path inside the remote `rootfs`.

Slurmy does not inspect arbitrary command arguments or configuration files for
more laptop paths. A literal path such as `/home/alice/tools/config.toml` in a
solver command remains unchanged and will normally be absent on the compute
node. Keep such resources under the solver root and refer to them relatively.
Similarly, selecting one problem file does not automatically discover files
that it includes. Put the complete problem and axiom tree under the solver root
when problems have such dependencies; the recursive packaging of the root will
then include them.

The solver description file itself is read while generating the submission and
does not need to exist on the cluster. `archive-paths.txt` records the local
paths that `submit.sh` packages later, so do not move or delete the solver root
or selected problems between generating and running `submit.sh`. For multiple
independent solver layouts, make one description file per root and repeat
`--solver`.

</details>

<details>
<summary><strong>🧩 How jobs are divided</strong></summary>

Slurmy expands every solver command over the selected problems. `--batch-size`
controls how many calls one array element runs sequentially. `--max-parallel`
limits how many array elements from this submission may be running at the same
time. It does not limit how many run in total. For example, with 1,000 batches
and `--max-parallel 100`, all 1,000 batches remain scheduled, but at most 100 can
be running simultaneously. Slurm may run fewer when cluster resources are busy.
The limit is not a machine count: Slurm may place multiple batches on one
machine when that machine has enough requested CPU and memory.

For 1,000 calls with `--batch-size 20 --max-parallel 10`:

```text
50 array elements
20 sequential solver calls per element
at most 10 elements running at once
```

Each individual call still gets its own runsolver CPU, wall-clock, memory, and
process-tree limits.

</details>

<details>
<summary><strong>🎛️ Limits and requests</strong></summary>

- `--cpu-limit`, `--wc-limit`, and `--mem-limit` limit each solver call through
  runsolver.
- `--cpu-request` and `--memory-request` request resources for each Slurm array
  element.
- `--memory-request` must be at least `--mem-limit` and should leave a little
  room for Bash and runsolver.

Memory values accept `MB`, `MiB`, `GB`, `GiB`, `TB`, or `TiB`. With no unit,
`MB` is assumed. CPU requests include `1-core`, `4-core`, `8-core`, `16-core`,
`32-core`, and `64-core`.

Extra `sbatch` options can be repeated. Use the joined form because the value
starts with `--`:

```bash
--sbatch-option=--partition=CPU-amd
--sbatch-option=--account=my-project
--sbatch-option=--qos=normal
```

Use `--host HOST` or `SLURMY_HOST` when the cluster SSH name is not `datalab`. Run
`./slurmy.py --help` for every option.

</details>

</blockquote>

</details>

<details>
<summary><strong>📂 Results</strong></summary>

The remote directory contains:

```text
$HOME/Slurmy/<job-id>/
  metadata.json
  submission.tsv
  batches/
  progress/
  rootfs/
  logs/
  results/
  slurm_job.sh
```

`logs/` contains Slurm output. `results/` contains:

- one tab-separated summary per batch;
- one compressed archive per batch attempt containing solver output,
  runsolver watcher data, variables, and controller output.

`submission.tsv` records the submitted Slurm array IDs and their batch ranges.
While a job is active, `progress/` records the task each array element is
currently running and how many calls it has finished. These small files drive
the live dashboard and remain readable without it.

The summary column names are listed in `metadata.json` under
`result_columns`. Important statuses are `ok`, `error`, `time-limit`,
`memory-limit`, `interrupted`, and `worker-error`.

Completed calls are skipped if an array element is rerun. Inputs are copied
under `rootfs/` with their original absolute paths mirrored there.

</details>
