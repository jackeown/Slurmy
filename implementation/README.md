# ⚙️ Implementation and command-line reference

Most users should start with the [web app](../README.md). This directory holds
the core runner, the generated-submission templates, remote build tooling, and
optional command-line utilities. The web app and workflow Makefiles use these
same components; this is not a separate execution model.

See the [full execution chain](execution-chain/README.md) for a diagram and
description of how creating a workflow leads to solver-problem pair execution
on the cluster.

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
| `problem` | Local problem file, or `tptp:/share/TPTP/TPTP-v9.2.1/Problems/...` for a shared cluster problem. |
| `wc_limit`, `cpu_limit` | Positive wall-clock and total CPU limits in seconds. |
| `mem_limit` | Positive memory limit, e.g. `500MB`, `2GiB`; a bare number means MB. |
| `cores` | Maximum physical cores for the call. |
| `threads_per_core` | Logical threads per physical core for the call. Optional; defaults to `1`. Slurmy rejects values above the partition's hardware limit. |
| `cpus` | Physical CPU sockets, or `auto` for cluster-derived placement; the web builder uses `auto`. |
| `exclusive_cpu`, `exclusive_node` | `true` or `false`: reserve whole sockets or an entire node. Empty values mean false. |

All columns except `solver_name`, `threads_per_core`, `exclusive_cpu`, and
`exclusive_node` are required. Use normal CSV quoting for
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
copies source to a compute node and runs the script from that copy. If source
and root are the same folder, the script may build normally in its working
directory: Slurmy copies the entire resulting folder back into the local root.
No Slurmy-specific environment variables are needed. If source and root differ,
the recipe writes selected runtime files to `$SLURMY_BUILD_OUTPUT`; Slurmy
downloads that output directory into the root. In either mode Slurmy checks
the declared artifact before packaging the job. A blank source lets an
output-mode recipe fetch its own source.
Independent resources build concurrently. Recipes run again on each dispatch;
there is no implicit build cache. Older two-line-per-resource files remain
accepted. The [build guide](build/README.md) covers standalone builds.

`resource_limiter_template.txt` is one shell invocation beginning with the
limiter executable path inside a declared resource root. The default is
runsolver. Its command should enforce the per-solver-problem pair limits and invoke
`{{solver_command}}`. For runsolver, a useful template is:

```text
/home/me/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --timestamp --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}
```

The log placeholders designate per-solver-problem pair files: runsolver's watcher log,
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

Direct `slurmy.py` preparation does not connect to the cluster and refuses to overwrite an
existing `submit.sh` or `submit.sh.files/`. Submission builds resources on
compute nodes, downloads their artifacts, packages local inputs, and submits
SLURM tasks. Dispatch checks threads per core over SSH before any build or
upload. Workflow Makefiles also check during `make prepare`, and the web
builder checks before previewing or saving. The generated `submit.sh.files/` directory holds readable remote
prepare/submit scripts, batch and solver-problem pair runners, solver-problem pair definitions, build
drivers, and metadata. Use the workflow's `make prepare-submit` to handle
regeneration when inputs change.

Each batch contains up to `--batch-size` solver-problem pairs, run sequentially. Different
batches can run concurrently across nodes. A batch contains matching core counts
and sharing/isolation requests, so a shared solver never inherits a dedicated
solver's socket or node allocation. Interleaved compatible requests are packed
together up to the batch size; dedicated requests do not create one batch per pair.
The batch's SLURM wall-time budget includes all its solver-problem pairs; memory
covers its largest pair. Socket/node isolation can allocate more physical cores
than the solver's affinity limit: a 12-core solver still uses 12 cores when SLURM
allocates an entire 32-core socket. The status page shows both counts. Slurmy uses a rolling submission
window (32 pending/running batches per job by default) so large jobs can fit
cluster submission limits. Set `SLURMY_MAX_OUTSTANDING` in a workflow Makefile
or pass `--max-outstanding` to `slurmy.py` to change it. Cluster-side submitters
coordinate under `~/slurmy/submit.lock`, divide available slots among active
Slurmy jobs, and count other jobs submitted by the same user. A configured
association limit is used when visible; submit-limit rejections are retried and
recorded as a temporary observed ceiling (not treated as a permanent exact
limit). Other users sharing an account or QoS may also affect that ceiling.

Remote jobs live under `~/slurmy/jobs/JOB_ID/`; build jobs live under
`~/slurmy/builds/`. Results publish per solver-problem pair, so completed solver-problem pairs can be downloaded
before the entire job finishes. A partially interrupted submission can be
resumed using its prepared files; earlier accepted batches and saved results
are retained. Remote job directories and build directories are private to
their owner (mode `700`).

Uploads are unpacked in a private staging directory and published by an
atomic rename only after completion. SSH failures during preparation/upload
are reported without dispatching a batch. Once `sbatch` begins accepting
batches, submission is checkpointed per batch rather than globally atomic:
SLURM cannot roll back allocations already accepted before a connection loss.
Inspect the reported job ID before attempting a retry.
The cluster submission controller keeps its SSH session connected until all
batches have been accepted (or submission is stopped). This supports clusters
whose login policy terminates all session processes at logout, including
`nohup`/`setsid` processes. Closing the browser does not close that session; keep
the local server and network connected while submission is in progress.
Already accepted SLURM tasks continue independently. A broken connection can
stop the controller; Resume retains accepted batches and completed results.
Its output is kept in `submission-controller.log` in the job directory.
Custom TPTP axioms are copied atomically by one Python process, with progress
every 500 files, rather than spawning separate `mkdir` and `cp` processes for
every axiom. Monitoring probes the controller's kernel lock on each refresh, including when
history data is cached. An abandoned submission with no active SLURM tasks is
shown as **Incomplete**, with a reason and a **Resume** action. While monitoring,
the local server checks known jobs every 30 seconds, even without an open
browser tab, and automatically resumes jobs it observed active (or with an
abandoned submission) after at least 15 seconds with no active SLURM tasks or
controller. Recovery appears in the progress toast. Explicitly stopped jobs and
old historical failures stay stopped. Automatic retries are spaced at least a
minute apart and stop after three attempts without new completed results.
Resume preserves
completed results and retries unfinished solver-problem pairs. A live solver or
a queued SLURM task can legitimately leave completion counts unchanged while it
runs or waits; unchanged counts alone are not grounds for killing or retrying it.

If the prepared submit script reports a connection loss during batch submission,
`SLURMY_RESUME_JOB_ID=JOB_ID bash ./submit.sh` resumes its checkpointed batch
list without rebuilding or re-uploading inputs.

Each solver-problem pair's result CSV includes `szs_status` and `szs_output`; for example,
`SZS output end Proof` records `Proof` in `szs_output`. The `time` member of a
solver-problem pair's original result archive measures the entire per-solver-problem pair limiter invocation,
including the solver. Runsolver's measurements of the solver tree replace that
fallback when available. Default download exports only `<job-name>_results-summary.csv`,
`<job-name>_solver-stdout.tgz`, and `<job-name>_solver-stderr.tgz` into `job-results/JOB_ID/`.
Each archive extracts into a same-named top-level directory and contains logs
named `<solver>___<original-problem-filename>___<solver-problem-pair-id>.log` (or
`.stderr.log` for stderr). The full problem filename is preserved exactly,
including its extension and characters such as `+` and `^`. Original archives
and diagnostics stay on the cluster. Use `slurmy-sync.py --verbose`
for readable per-pair archives under `job-results/verbose/JOB_ID/solver-problem-pairs/`
and original cluster files under `job-results/verbose/JOB_ID/diagnostics/`.
Each per-pair archive extracts into its own directory; the original archive
references and retry history are preserved in `diagnostics/results/`.

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
| `slurmy-sync.py` | Download the three-file job export into `job-results/JOB_ID/`; `--verbose` downloads raw files, and `--follow` keeps updating. |
| `slurmy-cancel.py` | Choose an active SLURM allocation to cancel, or provide its ID non-interactively. |
| `slurmy-job.py` | Stop or resume an entire Slurmy job by ID (`stop` or `resume`); resume keeps completed calls. |
| `check_threads.py` | Check requested threads per core against the selected partition before preparation or dispatch. |
| `build/slurmy-build.py` | Run one build recipe as a SLURM job and download its artifacts; see the [build guide](build/README.md). |
| `submit_coordinator.py` | Cluster-side helper that serializes `sbatch` attempts and fairly shares a user's available submission slots. It is packaged automatically; users do not run it directly. |
| `templates/workflow.mk` | Shared targets for every workflow Makefile; see the [workflow guide](../workflows/README.md). |

Each script has `--help`. The SSH alias defaults to `datalab`; `SLURMY_HOST`
or a script's `--host` option selects another alias. Use the web interface
unless you need a scripted workflow or lower-level diagnostics.

</blockquote>
</details>

### Monitoring Load and Refreshes

Monitoring reuses OpenSSH connections and serializes status collections per host.
Automatic browser polling respects the local cache: history and large jobs are
refreshed at most about once every 30 seconds. Previously loaded data remains
available while a refresh runs in the background, with its actual collection
time shown. Expired, invalidated, refreshing, or failed snapshots are explicitly
marked as saved data that may be out of date; their previous status is not a
claim about the current cluster state. Freshness metadata is tied to the exact
snapshot returned, even if a background refresh finishes during the response.
Polling does not stop at 100% while that snapshot still needs revalidation.
Failed collections retain that data and back off for a minute;
opening more tabs or repeatedly clicking Refresh does not bypass an in-flight
collection or the failure backoff. A first visit can show a loading message while
the same background collection continues.

Modern job detail refreshes read each result index once, reuse unchanged workflow
metadata and problem definitions, and fetch SLURM logs only when Inspect Log is
selected. Incremental updates still transfer changed metadata, including renamed
jobs. Accounting requests for a job are restricted to its recorded SLURM IDs.
Collectors also have a remote time budget, so local request timeouts do not leave
unbounded collectors running on the head node. Automatic recovery acts only on
newly collected observations, never on stale cached data.

### Interrupted Job Operations and Submission Diagnostics

Stop and Delete retry interrupted SSH connections or timeouts up to three times.
Each retry replays the complete operation script. Stop rechecks the queue after
cancellation, including when a task finishes during `scancel`. Delete rechecks the
queue and controller lock before removing files. Its separate lock and deletion
marker make a lost acknowledgment or partially completed removal safe to retry.
Missing jobs are acknowledged as already removed; symlink job directories are
rejected. Local downloads are removed only after successful cluster deletion.
Resume is not blindly replayed after an ambiguous connection failure: it may have
already launched work, so the message asks the user to check the refreshed status.

The job page now includes **Submission Diagnostic** when a controller fails,
showing the last 8 KiB of its log. History also shows its last output line. A
zero-batch failure is not enough to identify its cause without that output.
Controller failures also report the phase and number of accepted batches, including
failures before the first submission. A failed or timed-out coordinator `squeue`
read now returns WAIT: the controller pauses and retries after its normal five-second
delay without calling `sbatch` against an unknown queue. This does not retry an
ambiguous `sbatch` acceptance, or suppress permanent submission/validation errors.
Nested submission shells and detached controllers skip noninteractive startup
hooks while preserving exported variables such as PATH. The SSH login shell is
still managed by the user's cluster account; interactive-only commands such as
`bind` belong behind an interactive-shell check there. SSH security warnings are
retained in the full operation log rather than repeated in a large error notice.

Automatic-recovery monitoring uses a lightweight scheduler heartbeat for known
jobs. Running jobs require no result, metadata, or progress-file reads. Checks
are skipped when a collection is already active or a successful collection just
ran. Only confirmed idle candidates receive a fresh detailed collection before
recovery; saved cached results never authorize a resume. This replaces the older
background full-history scan while leaving normal history/detail views available.

Solver placement controls are ordered as whole node, whole CPU sockets, physical
cores, then threads per core (default 1). Changing whole-node isolation defaults
socket isolation to the same Yes/No choice; the socket choice remains editable.
Physical cores is a numeric dropdown populated from the selected partition's
compute nodes: 1 through cores-per-socket for dedicated sockets, or 1 through
sockets-per-node times cores-per-socket otherwise. Whole-node isolation does not
override this socket-based limit. One debounced lookup is shared across solver
cards; changing the host or partition checks again. An out-of-range saved value
requires a new choice. Older `all` editor values become the available maximum;
CSV/CLI definitions still accept `all` for backward compatibility. Numeric limits
continue to restrict the solver when isolation allocates a larger socket or node.

Submission controllers use `nohup setsid` with redirected streams: ignoring HUP
alone does not detach the process group from SSH/session cleanup. Resume always
uses the current launcher supplied by the client, including for older jobs with
an obsolete `remote_dispatch.sh` in their uploaded bundle. This was verified on
3 November 2026 against an account where a `nohup` probe did not survive closing
SSH but a `setsid` probe did. Its failed 64-batch job had repeatedly terminated
while preparing 2,438 custom axioms; resuming with the corrected launcher completed
that preparation and submitted the first 32 batches. SLURM then queued those tasks
for Priority, while the controller correctly waited at its 32-task limit.

Generated workflow file headers offer Download and Replace actions. Replacement
uses the workflow editor's validator and regenerates the definition and related
files together; invalid replacements leave the workflow unchanged. Relative paths
in uploaded definitions are resolved from the workflow directory. Replacing
`jobpairs.csv` switches to explicit pairs and removes obsolete configuration/glob
derivatives. Changes invalidate `submit.sh`, requiring preparation before the next
dispatch; submitted cluster jobs are unaffected. Example files can be downloaded;
duplicate the workflow before replacing them. Makefile replacement supports the
generated settings, with custom command/rule changes rejected rather than lost.


The job Download menu also offers **Download StarExec-Compatible Results**
(`slurmy-sync.py JOB_ID --starexec --host HOST`). It reuses the compact cluster
export and converts locally into two ZIPs under `job-results/starexec/JOB_ID`:
`JobN_info.zip` contains `JobN/JobN_info.csv`; `JobN_output.zip` contains
`JobN_output/Problems/DOMAIN/SOLVER___CONFIG/PROBLEM/PAIR_ID.txt`. N is a stable
numeric alias derived from the Slurmy job ID, and pair IDs retain Slurmy task IDs.
The column order and directory layout match the supplied StarExec Job7575 exports.

CPU/wall times are seconds and memory usage is bytes. A Slurmy solver configuration
name supplies both the solver and configuration columns because Slurmy does not
store a separate StarExec solver identity. TPTP-relative problem paths and original
filenames are retained; local problems go under Problems/local, with distinct
parent directories when names collide. Unknown expected results are `--`;
unfinished pairs are labelled incomplete. SZS mnemonics follow the
[TPTP ontology](https://szs.tptp.org/) and [manual](https://tptp-manual.tptp.org/).
Existing timestamped output lines are preserved. Other lines carry the final
recorded pair CPU/wall totals, explicitly labelled as totals rather than per-line
measurements. Stdout precedes stderr, with a separator explaining that the original
interleaving was not recorded. Standard and verbose downloads remain separate.
