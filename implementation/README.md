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
| `problem` | Local problem file, or `tptp:/share/datasets/TPTP/TPTP-v10.0.0/Problems/...` for a shared cluster problem. |
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
/home/me/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --timestamp --add-eof --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}
```

CPU, wall-clock and memory limits accept **0**, meaning no per-solver-problem-pair
limit for that measure. Core counts and threads per core must still be positive.
Use the included zero-aware runsolver (or a custom limiter supporting this
convention). Solver-specific command options may interpret zero differently.

When a limit is zero, each solver configuration can choose to keep its zero-valued
prover argument or remove it. The optional CSV columns `omit_cpu_limit`,
`omit_wc_limit` and `omit_mem_limit` accept `true`/`false` and default to false.
Removal applies only to the prover command, never to the resource limiter. It
supports `--option {{cpu_limit}}`, `--option={{cpu_limit}}`, short options and
quoted options such as `'-timeout({{cpu_limit}})'`; memory removal covers both
`{{mem_limit}}` and `{{mem_limit_mib}}`. Ambiguous shell expressions or options
mixing an omitted placeholder with another placeholder are rejected for manual
editing. A positive limit always keeps its argument, even if removal was selected.

A batch containing a zero wall limit requests **one week** (`--time=10080`).
A zero memory limit requests **one quarter of node memory**, using the selected
partition’s compute-node RAM; finite memory limits in the same batch must fit
that allocation. The outer per-pair wall-clock watchdog is disabled when the
wall limit is zero. These are finite SLURM batch allowances, not per-pair limits.
SLURM partition, account and QoS policies still apply. In particular, a one-week
request can remain pending if the partition allows less time; see the
[SLURM sbatch documentation](https://slurm.schedmd.com/sbatch.html).
CPU time alone does not impose a wall-clock limit. GetComputerInfo reports these
limits as supplied, using the new script’s `%` comment output and optional timestamps.

The log placeholders designate per-solver-problem pair files: runsolver's watcher log,
measurement variables, and captured solver output. The runner does not add
these flags behind your back. The template can also use solver placeholders.
The job inspector shows the exact rendered invocation and distinguishes empty
output from a file omitted by the limiter configuration.

By default, the bundled `tools/GetComputerInfo` runs on the compute node before
every solver-problem pair. Its hardware, operating-system and configured-limit
report appears at the start of the full runsolver output as TPTP comments,
without being added to the prover's raw stdout. Report lines use
`0.00/0.00` timestamps only when the limiter invocation contains `--timestamp`.
Collection runs outside solver timing and limits, with a 45-second timeout;
failure is recorded in the controller log and does not prevent the solver from
running. The SLURM wall-time budget includes this extra allowance. Disable it
with `GET_COMPUTER_INFO := false` in the workflow Makefile or
`--no-computer-info` when preparing directly with `slurmy.py`.

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
All ordering modes group by the same SLURM allocation settings: physical cores,
threads per core, requested socket count, and dedicated socket/node settings.
Solver name, resource directory, command, per-pair limits, and limit-argument
omission settings do not prevent pairs from sharing a batch. Each pair retains
its own command and limits. Partition and reservation apply to the whole job.

Slurmy scans `jobpairs.csv` in its saved order and fills an open batch for each
allocation type, collecting compatible rows even when incompatible rows appear
between them. For example, A1, B1, A2, B2 with matching allocation settings and
batch size 2 becomes [A1, B1] and [A2, B2]. With different allocation settings for
A and B, it becomes [A1, A2] and [B1, B2]. Every row belongs to exactly one batch,
and rows retain their order within each allocation type. Solver-major ordering
still tends to keep a solver's pairs together, but its final partial batch can
be filled by another compatible solver. Ordering controls which compatible rows
fill batches first; it does not introduce extra configuration boundaries.

For a dispatched job, expand **Batches** on its job page, then expand a batch to
see its solver-problem pairs in execution order, statuses, and output links. The
inspector uses saved membership rather than dividing pair IDs by the batch size;
batch IDs start at 0 and match the submission plan. Batch lists and pair lists are
paginated and load only when expanded, reusing the job's cached snapshot. Stale
snapshots and estimated membership for older jobs are labelled explicitly.
The batch's SLURM wall-time budget includes all its solver-problem pairs; memory
covers its largest pair. Socket/node isolation can allocate more physical cores
than the solver's affinity limit: a 12-core solver still uses 12 cores when SLURM
allocates an entire 32-core socket. The status page shows both counts. Slurmy uses a rolling submission
window (50 pending/running batches per job by default) so large jobs can fit
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
`local_dispatch.py` drives finite submission waves from the laptop. Each wave
executes `remote_dispatch.sh` and `remote_submit.sh` in a connected SSH session,
yields when the submission window fills, and exits; the local process waits
between waves. No detached head-node submitter or compute-node controller is
launched. Keep the local server running and the laptop awake until every batch
has been accepted. Already submitted tasks continue after disconnecting. Resume
upgrades the coordinator/remote submission helpers on older jobs and returns
the saved context to the local driver; completed results and the unfinished-pair
worklist are preserved.

`slurm_api.py` provides the optional per-SSH-host REST connection, configured from
**Connection ▾** in the app. Settings live in `~/.config/slurmy/connections.json`
(mode 600); JWTs are held only in process memory. `SLURMY_CONNECTIONS_FILE` can
select a separate settings file for testing. Token bootstrap uses one SSH request
with `scontrol token lifespan=3600`; scheduler HTTP calls use the user name and
JWT headers. Tokens renew on expiry or an authentication rejection. Mutations are
never blindly replayed after transport failures and redirects are rejected.
REST status and hardware queries run from the laptop; the existing cluster view
still reads private solver progress over SSH when expanding a node. Historical
accounting and job files also retain their SSH access.

REST submission bridges scheduler requests from the short remote preparation
wave to `local_dispatch.py` over framed messages. The per-user filesystem
lock stays held across the queue check and POST, preserving fairness between
SSH and REST clients. Every generated resource/placement option is translated;
unsupported options fail explicitly. Queue waits use HTTP locally without a held
SSH session. The selected account’s reported/observed cap, configured per-job
window, and other jobs in that account constrain dispatch. The default account
retains its 50-task fallback; custom accounts do not inherit it. Accounting-policy
queries still run through SSH (cached for five minutes), rather than inventing a
REST limit. Batch plans are reused across waves.

The previous compute-node controller and detached launchers are retained under
[`archived/`](archived/README.md) for reference, with regression coverage, but
are not packaged into new submissions.
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

All 53 **Success** and 29 **NoSuccess** values published at
[the SZS ontologies](https://szs.tptp.org/) are recognized by full name and
three-letter mnemonic, case-insensitively. The shared catalog is
[`szs.py`](szs.py), initially checked on 16 November 2026. Its 53 Success
name/code mappings were also checked against the official
[`SZS_Status.thy`](https://github.com/TPTPWorld/SZSOntologies/blob/master/SZS_Ontologies/SZS_Status.thy)
on 18 November 2026. Translation is an offline lookup, not a network request or
a guessed abbreviation. `Theorem` maps to `THM`, `CNFRefutation` to `CRf`, and the
StarExec `SZSResult` joins them as `THM-CRf`. Workers, historical monitoring,
charts, filters and downloads normalize equivalent spellings to the official
full name. Acronyms also work in status searches and solver-outcome filters.
A standalone three-letter code searches that exact status category, so `CTO`
finds CPUTimeout without also matching the text of ContradictoryAxioms.
Unknown future values are preserved, rather than treated as recognized success.

Finishing a solver-problem pair is separate from reporting an SZS Success.
`GaveUp`, `Unknown`, `Incomplete` and other non-error NoSuccess answers finish
the attempt but do not count as SZS success. Error statuses retain their precise
names and are classified as execution issues. `CPUTimeout` and `WCTimeout` retain
their distinction, including their CPU/wall timeout classification in StarExec
exports. `Assumed(Unknown,Theorem)` / `ASS(UNK,THM)` is a NoSuccess value and is
never counted as an established theorem. Download metadata's historical
`solved_tasks` field counts completed attempts with a Success ontology value;
this includes successful processing/verification and is not exclusively a proof count.

The separate DataForm catalog is also supported by full name and mnemonic;
`SAT` in a status means Satisfiable, while `Sat` in an output field means
SaturationHerbrandModel. Standard summary downloads include the additional
`szs_status_code` and `szs_output_code` columns; existing columns stay in place.
StarExec downloads use the official mnemonics throughout.

Each solver-problem pair's result CSV includes `szs_status` and `szs_output`; for example,
`SZS output end Proof` records `Proof` in `szs_output`. The `time` member of a
solver-problem pair's original result archive measures the entire per-solver-problem pair limiter invocation,
including the solver. Runsolver's measurements of the solver tree replace that
fallback when available. Default download exports `<job-name>_results-summary.csv`,
`<job-name>_solver-stdout.tgz`, `<job-name>_solver-stderr.tgz`, and
`<job-name>_postprocessing.tgz` into `job-results/JOB_ID/`.
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
when available; `SLURMY_TPTP_ROOT` can override the implicit default. An explicitly
selected full-collection axiom glob takes precedence over that environment setting.

</blockquote>
</details>

<details>
<summary><strong>🧰 Other scripts</strong></summary>

<blockquote>

| Script | Purpose |
| --- | --- |
| `slurmy-pairs.py` | Expand configurations × all selected problem globs into an explicit `jobpairs.csv`; supports problem-major, solver-major, or reproducible random order. |
| `slurmy-sync.py` | Download the summary and three log archives into `job-results/JOB_ID/`; `--verbose` downloads raw files, and `--follow` keeps updating. |
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

Current submission never uses `nohup`, `tmux` or a detached head-node process.
Earlier probes of session cleanup are historical and do not establish that a
login-session process will survive the cluster’s current logout policy. See the
archived controller notes for the distinct compute-node approach.


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
Like StarExec, new runsolver workers capture stdout and stderr together through
runsolver's live timestamping pipe. StarExec downloads copy that combined stream
verbatim, including its native EOF; stderr is not appended a second time. Separate
raw stdout/stderr copies remain available in the interface and standard downloads.
The output inspector defaults to **Runsolver output (stdout + stderr)** when that
recording exists; **Solver stdout (raw)** and **Solver stderr** remain separate
choices. This exposes the native per-line timestamps for existing saved runs too.
Those copies retain the solver's own formatting rather than runsolver timestamps;
the independently collected computer-information header is prepended only to
the combined full-run recording.
Two small `tee` processes drain the streams during execution and are awaited before
publishing results. Timestamps reflect when runsolver receives output; Slurmy
cannot force a solver to flush its own internal buffers or recover a total order
of simultaneous writes from different processes. This capture applies to limiter
templates using `--solver-data {{solver_log}}`; custom limiters retain their
existing capture behavior.

Older runs have no combined recording. Their timestamped stdout is preserved,
then stderr is appended with final pair CPU/wall totals for untimestamped lines
as a compatibility fallback, not per-line measurements. Their original stderr
timing/interleaving cannot be recovered. A recorded stdout EOF, if present, is
retained last with its original timestamp. Downloading reports this limitation
outside the output files. The exporter adds no commentary or artificial EOF.
The bundled runsolver fixes a missing `break` after the `--add-eof` handler in
the supplied source; builds exercise these options and verify a timestamped EOF.
The bundled prebuilt binary was rebuilt on datalab on 18 November 2026 and
passes the native EOF smoke test. New defaults and bundled example workflows use
`--timestamp --add-eof` in that order. Add `--add-eof` after `--timestamp` in
existing custom invocations too; previously collected logs are not given an
artificial EOF when downloaded.
Standard and verbose downloads remain separate.

## Workflow Builder and REST Transport

New workflows directly define solver configurations paired with all selected
problem sources. CSV replacement remains available through generated workflow
files; older explicit-pair definitions keep their saved semantics when edited.
Problem sources can be copied from another workflow, with duplicate globs omitted.
The axiom editor uses the same source rows and local/cluster browsing as problems,
prefilled with `tptp://TPTP-v10.0.0/Axioms/**/*.ax`. Existing custom sources are
retained when editing; workflows with implicit shared axioms show the default glob.
A single full-collection glob uses the selected shared collection directly, even
when problems come from another version, avoiding copies of the whole axiom set.
Replace the row for a different collection or subset, or add rows to combine sources.
Both problem and axiom sources can be copied from existing workflows; the picker
previews their globs, keeps current rows, and skips duplicates already selected.
Local and cluster TPTP browsing are supported for both problems and custom axioms.
Custom shared axioms use `tptp://VERSION/Axioms/...`, are copied on the cluster
into the job-local axiom tree, and are included as real files in portable exports.
Picker addresses navigate on Enter or blur.

The submitted-task window defaults to 50. The unified batch suggestion is the
larger of the conservative half-capacity size and the smallest size that fits all
allocation groups into the currently available submission window.
The capacity estimate is `max(1, floor(hardware parallelism / 2))`, then capped
by the workflow submission window and any reported account submission limit
(50 for the default account); the starting size is the pair count
divided by that assumed parallelism, rounded up. The factor of two allows for
other cluster users rather than aiming for two waves.
If no size can fit, the explanation and submission warning say that later batches
must wait for the local submitter. The explanation exposes capacity arithmetic
and the current values; the estimate is not a promise of immediate scheduling.

REST mode now covers build submission/status and Stop/Resume/Delete scheduler
operations, as well as worker submission, queue monitoring and cluster status.
SSH still transfers files and reads private results. Lifecycle scripts request
fresh queue snapshots through the local HTTP bridge rather than silently using
SSH scheduler commands. API cancellation errors are checked even when HTTP itself
succeeds. Independent API reads no longer serialize behind one global cache lock.
The v0.0.40 distribution field cannot be parsed by datalab's server; single-task,
single-node submissions omit that redundant field rather than send an ignored
placement setting. Source builds translate SLURM duration syntax to API minutes.


## SLURM Accounts

On Basics, **SLURM account (optional)** selects the account used for worker and
build jobs. Leave it blank for the cluster default, or enter `cpuopt` if your user
has that association. The Makefile equivalent is `SLURMY_ACCOUNT := cpuopt`.
Slurmy passes `--account=cpuopt` to `sbatch` and the corresponding account field to
REST submissions. The selected account is saved for Resume and portable workflows.
An invalid or unauthorized account produces a submission error; Slurmy does not
silently retry under the default account.

Datalab's default account reports a 50-task submission limit. On 19 November 2026,
`cpuopt` reported no account-specific MaxSubmitJobs limit for the configured user.
Slurmy scopes queue counts, fair sharing, cached policy and observed submission
limits to the selected account. Other SLURM/QOS policies still apply. If no limit
is reported for a custom account, Slurmy uses the workflow submission window
without imposing a fictitious 50-task account cap. The window still defaults to
50: increase **Maximum submitted SLURM tasks for this job** to submit more batches
at once. Suggestions and the submission warning use the same account-specific
policy; cached/unknown policy is an estimate, not permission to bypass SLURM.


## Text Workflow Definitions and Build Scripts

`workflow.json` is the visible text definition in a saved workflow. It contains
solver configurations, problem/axiom sources, build settings, limits and SLURM
settings, but no SSH host. The app always uses the host selected in its header.
Older hidden `.slurmy-workflow.json` definitions remain readable. The JSON file
appears alongside the other workflow files: edits/replacements through that UI
regenerate derived CSV, Makefile and build files atomically. Editing those files
through their controls updates the JSON too. Use the rename control to rename
an existing workflow; invalid JSON leaves the saved workflow unchanged.

For a text-only workflow, author or edit `workflow.json` in its workflow directory
and run `python implementation/slurmy-workflow.py PATH/workflow.json --host datalab`
to regenerate its files (`--check` validates without replacing them). The host is
an invocation setting, not part of the definition. Local paths and script paths
may be absolute; quote script filenames containing spaces before appending
arguments, for example `"/path/build solver.py" --configuration release`.
Scripts are staged to the compute node and run with their shebang interpreter;
scripts without a shebang retain the legacy Bash fallback. Arguments are literal
shell-quoted words, are preserved in portable workflows, and invalidate build
reuse when changed. Shell pipelines belong inside a script or the explicit
inline-command build option.

Batch suggestions also respect a six-hour planning budget. For finite limits,
the conservative maximum is `floor((21600 - 60) / (largest wall limit + overhead))`,
where overhead is 80 seconds/pair with computer information or 30 without it.
This matches the generated startup and per-pair allowances. If the time cap
requires more batches than the submission window, later batches must wait.
A zero wall limit is unbounded and cannot provide a six-hour completion guarantee;
even a single finite pair can exceed the allocation when its own wall limit is
longer than the batch budget. The explanation and manual-size warning show these
cases rather than claiming a guaranteed fit.

CPU-amd was verified on 19 November 2026 as 20 nodes, two 32-core sockets/node,
one thread/core, `CR_CORE_MEMORY`, `task/cgroup,task/affinity`, `OverSubscribe=NO`,
and `MaxTime=06:00:00`. A 15-core allocation owns those cores while other jobs may
use remaining cores. A 64-core request consumes all configured CPU capacity on
one node even without `--exclusive`; explicit whole-node exclusivity additionally
expresses the placement/isolation policy. Selecting a dedicated node suggests
all node cores with dedicated sockets off; selecting dedicated sockets suggests
one socket's cores. Existing saved core choices are kept until the user changes
placement.

The workflow editor discovers SLURM account memberships for the currently selected
SSH user using a read-only `sacctmgr show association` query. Results are cached
per host for five minutes. With one account, it is selected and the control is
hidden. With two accounts, the non-default account is preferred. With three or
more, `default` is preferred if available; otherwise the first account
alphabetically is used. Saved accessible choices are preserved. Only actual
memberships appear in the selector; there is no extra 'Cluster default' option. Lookup failures show a retry action
and do not masquerade as default-only membership. Portable imports preserve an
available account preference or select an available replacement for the importing
user, updating both JSON and Makefile. Membership discovery is required during
GUI import, so an unavailable cluster produces a retryable import error instead
of silently keeping an unauthorized account.

In the workflow editor, the solver command is checked locally without executing
it. Missing files and missing execute permission are shown under **Command** and
must be corrected before continuing. A configured cluster build may produce its
expected executable later; that case is identified explicitly instead of being
rejected as a missing prebuilt solver.

## Inspecting a Preparation Before Dispatch

On a workflow page, choose **Prepare only**, then open **Generated Workflow Files →
Prepared Batch Plan**. Expand a batch to see its exact solver-problem pair IDs,
solver configurations and problem filenames in execution order. Page sizes are
configurable, and **Download batch plan** exports the complete mapping as CSV.
This reads `submit.sh.files/metadata.json` and `manifest.jsonl` locally, without
submitting anything or contacting SLURM. Preparing again replaces this local plan;
submitted jobs retain their own saved plans. Editing the workflow invalidates its
preparation, so prepare again before inspecting an updated plan.

## Selecting and Ordering Problem Filenames

Each problem source has **Select and order specific filenames (optional)**.
Choose **Disabled** to use every matched problem, **Problem names** to type one
name per line, or **Files of problem names**
to import multiple text files. Files are concatenated in the displayed order;
only the file picker is shown in file mode, and the imported names are saved
inside the workflow, so exports
do not depend on the original list files. Absolute paths match the full problem
path exactly (cluster paths may use `/share/...`); relative names match only their
last filename component. Enter names in the desired execution order. Names must
match exactly one file in that source; missing or ambiguous names are errors.
Leave the list blank to use all matches. This works for local globs, archives and
cluster TPTP globs. The ordered lists are saved in `workflow.json` and generated
`problem-selection.json`; the preparation command applies them too. Problem-major
order follows the selected problem sequence across solvers, while solver-major
order follows that sequence within each solver. Duplicate problems across sources
are used once, at their first selected occurrence. Portable workflows preserve the
selected problems and order.

GetComputerInfo now requests only Model, CPUModel, RAMPerCPU, OS, CPULimit,
WCLimit, MemLimit and DateTime, using `-p --prefix '% '`. Its computer-name header
is retained, and `--timestamp` is added only when enabled for the limiter.

Account discovery is advisory: a slow or failed lookup no longer prevents moving
through the workflow editor. Until verified, the saved account or cluster default
is retained. Failed REST topology reads fall back to read-only SSH hardware checks;
submission itself still uses the selected scheduler transport and reports its own
authentication errors.

## Parallel Cluster Builds

Workflow resource builds request one CPU socket’s worth of physical cores from
the selected partition (currently 32 cores on CPU-amd), with one hardware thread
per core. This is a core-count allocation, not a dedicated-socket reservation.
The build job exports `MAKEFLAGS` with `-j` and `CMAKE_BUILD_PARALLEL_LEVEL`, so
Make and CMake builds can compile in parallel without changing their scripts.
Other build tools can use `$SLURM_CPUS_PER_TASK` or `$SLURMY_BUILD_CORES` for their
worker count. Explicit parallelism in a recipe should stay within that allocation.
Existing build-cache policies still apply. Run **Prepare only** again for an
already-prepared workflow to regenerate its build launcher with this setting.
The standalone build driver also supports `--cpus-per-task=socket` with an explicit
`--sbatch-option=--partition=NAME`; its numeric option remains available.

## Solver Environment Variables

Each solver configuration accepts optional **Environment variables**, for example:

```text
X=Y Z=3
Hello = World
GREETING="Hello World"
EMPTY=
THREADS={{cores}}
INPUT={{problem}}
```

Assignments may be separated by spaces or newlines, with spaces around `=`.
Names use letters, digits and underscores and cannot start with a digit. Quote
values containing whitespace; single and double quotes are supported, and a
backslash escapes the next character outside single quotes. Empty values are
allowed. Duplicate names, malformed assignments, control characters and unfinished
quotes or escapes are errors, shown beneath the field. Values support the same
solver-command placeholders: `{{problem}}`, `{{cores}}`, `{{cpu_limit}}`,
`{{wc_limit}}`, `{{mem_limit}}` and `{{mem_limit_mib}}` (also `{{cpus}}` when
explicitly configured). Each pair receives its own limits and cluster problem
path, including paths with spaces. Unknown or malformed placeholders are errors.
Other text remains literal: shell expansion and command substitution are never
performed on environment values.

Variables are exported inside the solver process wrapper, so the solver and its
children receive them without changing other pairs, the resource limiter or build
jobs. Solver commands can reference them normally, such as `"$GREETING"`. They
are preserved in `workflow.json`, the optional `environment` CSV column, copied
configurations and portable workflow bundles. Older CSVs may omit the column.

## Submitting Batches as SLURM Arrays

Each Slurmy batch remains an independently scheduled SLURM array element. The
submitter combines consecutive batches only when their complete generated SLURM
requests match, including memory, wall time, cores, placement, account and
reservation. Batch membership, commands, limits and result filenames are unchanged.
Different requests and gaps in a resume worklist start separate arrays. Arrays
are split at the cluster's `MaxArraySize` / `max_array_tasks` limits and at a
conservative maximum of 1000 elements per submission.

The coordinator counts **elements**, including pending elements, against the
workflow window, account limits and fair-share targets. It trims the next array
to fit currently available capacity. A definite submission-limit rejection may
be retried as a smaller array; an ambiguous transport failure is not replayed.
Arrays therefore reduce submission requests without bypassing the 50-job limit
on the default account. The laptop must still remain available until all batches
have been accepted; accepted elements continue independently of the laptop.

`submission.csv` retains one row per accepted batch. The original five columns
remain, with an additional `array_task_id` column recording the local element
index. `offset + array_task_id` gives the stable Slurmy batch ID; several rows
can share the same parent SLURM ID and offset. Old five-column rows remain valid.
The unique scheduler identity and log name use the parent **and** element index
(e.g. `12345_2`, `slurm_12345_2.out`). Batch progress, results, downloads and
stop/resume continue to use each batch's original identity. Resume upgrades the
small scheduler helper scripts for older jobs and preserves completed results.


## Postprocessing Solver Output

In **Solvers → Workflow Postprocessing**, select the bundled SZS parser, a custom
postprocessor, or **None**. Solvers inherit this workflow setting unless you check
**Override workflow postprocessing for this solver**. Custom software is specified
by its local folder and entrypoint command, for example `python3 parse.py`,
`bash parse.sh`, or `./parser`. The folder is uploaded with the workflow's other
resources. Commands run from that folder on the compute node after the prover
finishes. Slurmy appends two quoted filename arguments: raw prover stdout first,
then raw prover stderr. The files exist even when a stream is empty.

Postprocessor stdout may contain any report text. Lines of this form also define
result fields; whitespace around `=` is optional:

```text
SZS data status = sat
SZS data score = 42
SZS data note = quotes, commas and = signs are literal values
```

Names start with a letter or underscore and may then contain letters, numbers,
underscores, dots or hyphens (up to 128 characters). Values are text, and the last
assignment to a name wins. `status` supplies the solver-problem pair's displayed
outcome, preserving custom names such as SMT-LIB `sat` or `unsat`. Other fields
appear under **Result details** and become columns in the standard CSV, with
blank cells for pairs that do not supply them. The CSV retains `execution_status`
separately. Custom names colliding with protected CSV columns receive a
`postprocess.` prefix rather than replacing measurements or identifiers.

Stdout and stderr from postprocessing are inspectable with each pair's other
output streams. Standard downloads include a fourth file,
`<job-name>_postprocessing.tgz`, containing the raw reports and diagnostics inside
a same-named directory, using the same solver/problem/pair filename convention.
Verbose downloads retain these files in the individual pair archives as well.

The bundled example lives at `implementation/postprocessors/default_szs.py`. It
recognizes SZS statuses and output forms (including official three-letter names)
and emits `SZS data status=Theorem` and, when present, `SZS data output=Proof`.
Prover-output recognition is implemented there rather than hardcoded in the
worker. Existing SZS conversions and StarExec downloads remain available.

The default postprocessor timeout is 30 seconds, configurable from 1 to 600.
Batch wall-time planning includes that timeout plus a five-second cleanup
allowance for each active postprocessor. Solver CPU/wall/memory measurements
exclude postprocessing. A failing, timed-out or cancelled postprocessor keeps the
finished solver result and original logs, while exposing its exit code and
error separately. Disabling postprocessing adds no processing allowance.
Workflow definitions, generated CSVs, duplication and portable export/import
retain the effective processor and any solver overrides.
