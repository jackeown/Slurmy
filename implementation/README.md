# ⚙️ Implementation and command-line reference

Most users should start with the [web app](../README.md). This directory holds
the core runner, the generated-submission templates, remote build tooling, and
optional command-line utilities. The web app and workflow Makefiles use these
same components; this is not a separate execution model.

<details>
<summary><strong>📄 The three input files</strong></summary>

<blockquote>

`slurmy.py` consumes `jobpairs.csv`, `building.txt`, and
`resource_limiter_template.txt`. A [workflow Makefile](../workflows/README.md)
can prepare them for you. Paths in a handwritten input file resolve relative
to that file; the web app normally saves absolute paths.

`jobpairs.csv` has one row per solver–problem call. Its columns are:

| Column | Meaning |
| --- | --- |
| `solver_name` | Optional display label for this solver configuration. |
| `command` | Solver shell command; may use the placeholders below. |
| `solver_directory` | Local runtime directory, copied to the cluster and used as the command's working directory. |
| `problem` | Local problem file for this call. |
| `wc_limit`, `cpu_limit` | Positive wall-clock and total CPU limits in seconds. |
| `mem_limit` | Positive memory limit, e.g. `500MB`, `2GiB`; a bare number means MB. |
| `cores` | Maximum physical cores for the call. |
| `cpus` | Physical CPU sockets, or `auto` for cluster-derived placement; the web builder uses `auto`. |
| `exclusive_cpu`, `exclusive_node` | `true` or `false`: reserve whole sockets or an entire node. Empty values mean false. |

All columns except `solver_name` are required. Use normal CSV quoting for
commands containing commas, quotes, or newlines. Solver-command placeholders
include `{{problem}}`, `{{wc_limit}}`, `{{cpu_limit}}`, `{{mem_limit}}` (decimal
MB), `{{mem_limit_mib}}` (MiB), and `{{cores}}`. Explicit numeric socket counts
also permit `{{cpus}}`. Leave path placeholders unquoted: the runner quotes
them. Commands without placeholders are valid, but their hard-coded limits
should agree with the row's limits.

`building.txt` is normally a JSON list. Each item declares a local runtime
`root` and, optionally, a `source` directory, Bash `recipe`, and executable
`artifact` path relative to the root:

```json
[
  {"root": "/home/me/solver/bin", "source": "/home/me/solver/src",
   "recipe": "/home/me/solver/build.sh", "artifact": "solver"},
  {"root": "/home/me/runsolver", "source": "",
   "recipe": "", "artifact": ""}
]
```

With no recipe, Slurmy packages the existing root as-is. With a recipe, it
copies source into `$SLURMY_BUILD_WORK` on a compute node. The recipe must put
the declared artifact under `$SLURMY_BUILD_OUTPUT`; Slurmy verifies it,
downloads the output directory into the declared local root, then packages
that root for the job. A blank source lets a recipe fetch its own source.
Independent resources build concurrently. Recipes run again on each dispatch;
there is no implicit build cache. Older two-line-per-resource files remain
accepted. The [build guide](build/README.md) covers standalone builds.

`resource_limiter_template.txt` is one shell invocation beginning with the
limiter executable path inside a declared resource root. The default is
runsolver. Its command should enforce the per-call limits and invoke
`{{solver_command}}`. For runsolver, a useful template is:

```text
/home/me/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --timestamp --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}
```

The log placeholders designate per-call files: runsolver's watcher log,
measurement variables, and captured solver output. The runner does not add
these flags behind your back. The template can also use solver placeholders.
The job inspector shows the exact rendered invocation and distinguishes empty
output from a file omitted by the limiter configuration.

</blockquote>
</details>

<details>
<summary><strong>🚀 Prepare and submit without the web app</strong></summary>

<blockquote>

From a directory containing the three input files:

```bash
python /path/to/Slurmy/implementation/slurmy.py jobpairs.csv building.txt resource_limiter_template.txt --batch-size 20
bash ./submit.sh
```

Preparation does not connect to the cluster and refuses to overwrite an
existing `submit.sh` or `submit.sh.files/`. Submission builds resources on
compute nodes, downloads their artifacts, packages local inputs, and submits
Slurm tasks. The generated `submit.sh.files/` directory holds readable remote
prepare/submit scripts, batch and call runners, call definitions, build
drivers, and metadata. Use the workflow's `make prepare-submit` to handle
regeneration when inputs change.

Each batch contains up to `--batch-size` calls, run sequentially. Different
batches can run concurrently across nodes. A node-exclusive call gets its own
batch. The batch's Slurm wall-time budget includes all its calls; memory and
core reservations cover its largest call. Slurmy uses a rolling submission
window (32 pending/running batches by default) so large jobs can fit cluster
submission limits. `SLURMY_MAX_OUTSTANDING` changes that window.

Remote jobs live under `~/slurmy/jobs/JOB_ID/`; build jobs live under
`~/slurmy/builds/`. Results publish per call, so completed calls can be synced
before the entire job finishes. A partially interrupted submission can be
resumed using its prepared files; earlier accepted batches and saved results
are retained. Remote job directories and build directories are private to
their owner (mode `700`).

Relative solver commands run from their `solver_directory` copy. Local paths
for listed problems and packaged resources are translated into the cluster's
`rootfs/`; paths embedded inside a larger shell argument are not rewritten.
Keep runtime dependencies within declared resource roots. For TPTP
`include('Axioms/NAME.ax')`, `--axioms-file` packages custom axioms and points
`TPTP` at their parent. Otherwise Slurmy uses the cluster's shared TPTP root
when available; `SLURMY_TPTP_ROOT` can override it.

</blockquote>
</details>

<details>
<summary><strong>🧰 Other scripts</strong></summary>

<blockquote>

| Script | Purpose |
| --- | --- |
| `slurmy-pairs.py` | Expand configurations × all selected problem globs into an explicit `jobpairs.csv`; supports problem-major, solver-major, or reproducible random order. |
| `slurmy-sync.py` | Incrementally rsync a job's results into `job-results/JOB_ID/`; use `--follow` to keep updating. |
| `slurmy-cancel.py` | Choose an active Slurm allocation to cancel, or provide its ID non-interactively. |
| `build/slurmy-build.py` | Run one build recipe as a Slurm job and download its artifacts; see the [build guide](build/README.md). |
| `templates/workflow.mk` | Shared targets for every workflow Makefile; see the [workflow guide](../workflows/README.md). |

Each script has `--help`. The SSH alias defaults to `datalab`; `SLURMY_HOST`
or a script's `--host` option selects another alias. Use the web interface
unless you need a scripted workflow or lower-level diagnostics.

</blockquote>
</details>
