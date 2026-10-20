# 🧭 Workflows

A workflow is a reusable definition; dispatching it creates a new job. The web
app is the easiest way to create, edit, duplicate, and submit one. It saves
user-defined workflows in `my-workflows/` and lists the supplied examples
separately. Saving does not run anything.

<details>
<summary><strong>📁 What is here?</strong></summary>

<blockquote>

- [`example-workflows/`](example-workflows/README.md) contains ready-to-run
  Vampire, E, Drodi, and combined workflows, plus an editable template.
- [`example-solvers/`](example-solvers/README.md) contains their source
  snapshots; build scripts compile them on cluster compute nodes.
- [`example-problems/`](example-problems/README.md) contains small sample TPTP
  problems.
- [`my-workflows/`](my-workflows/README.md) contains workflows created in the
  web app. Those user folders are ignored by Git.

</blockquote>
</details>

<details>
<summary><strong>📄 Files in a saved workflow</strong></summary>

<blockquote>

The web app writes a folder such as `my-workflows/my-study/`. The principal
files are:

```text
my-study/
  Makefile                       # Shared prepare, submit, monitor, and sync targets.
  jobpairs.csv                   # One solver–problem call per row.
  building.txt                   # Runtime roots and optional cluster build recipes.
  resource_limiter_template.txt  # Command wrapping each solver call.
  .slurmy-workflow.json          # Form choices used when editing in the web app.
```

For the all-pairs convenience mode, the folder also contains
`configurations.csv` and `problem-globs.txt`; the Makefile expands their
configuration × problem product into `jobpairs.csv`. Optional
`axiom-globs.txt` lists custom TPTP axiom inputs. Build commands typed in the
form are saved as `build-resource-*.sh`. The solver source and problem files
can remain at their existing paths; they are packaged at submission time.
Problems can be selected by files, globs, ZIP/tar archives, or a cluster TPTP
pattern such as `tptp://TPTP-v9.2.1/Problems/*/*.p` (all domains) or
`tptp://TPTP-v9.2.1/Problems/AGT/*.p` (one domain). Cluster TPTP problems remain under the
shared `/share/TPTP/TPTP-v9.2.1/Problems/` tree. If custom axioms
are selected, they replace the default shared TPTP library for that job.

The three core inputs are `jobpairs.csv`, `building.txt`, and
`resource_limiter_template.txt`. Each CSV row is one explicit solver-problem pair; Slurmy
does not silently expand it into other calls. See the
[input format and runner reference](../implementation/README.md) for details.

</blockquote>
</details>

<details>
<summary><strong>🔧 The Makefile setup</strong></summary>

<blockquote>

Workflow Makefiles set a few variables and include the shared rules in
`implementation/templates/workflow.mk`. For example:

```make
REPO_ROOT := ../../..
BATCH_SIZE := 20
SLURMY_MAX_OUTSTANDING := 32
BUILD_RECIPES := build-solver.sh
include $(REPO_ROOT)/implementation/templates/workflow.mk
```

`REPO_ROOT` is the path from the workflow folder to this repository.
`BATCH_SIZE` is the maximum number of solver-problem pairs in one SLURM task; those solver-problem pairs run
**one at a time** within that task, while different tasks may run on different
nodes. `SLURMY_MAX_OUTSTANDING` caps the pending/running batches from one job;
the default is 32, and any positive value is allowed. Slurmy also coordinates
concurrent submissions under the same cluster Unix user and shares available
slots fairly; a SLURM-reported or observed submit limit takes precedence.
The coordinator counts all of your queued and running SLURM allocations,
including non-Slurmy jobs, before each `sbatch`. Active Slurmy submitters
divide the remaining capacity fairly: a small job's unused share goes to
other waiting jobs. A submit-limit rejection records a temporary observed
ceiling and is retried; the batch is not discarded. The per-job setting is a
ceiling on *simultaneously pending or running* batches, not on the total
number that can run over the lifetime of a job.

`BUILD_RECIPES` lists build scripts whose changes should trigger fresh
submission-file generation. All-pairs workflows add a rule to create
`jobpairs.csv` from configurations and problem globs; see the
[examples](example-workflows/README.md).

| Target | What it does |
| --- | --- |
| `make` or `make prepare` | Check the selected partition's thread topology over SSH, then create or refresh local `submit.sh` and its helper files. No job is submitted. |
| `make prepare-submit` | Prepare if needed, then build declared resources and submit a new job. |
| `make submit` | Dispatch the **already prepared** files without regenerating them; still rebuild declared resources. |
| `make build` | Build and download declared resources without submitting the proving job. |
| `make monitor` | Start or reuse the web app and open this workflow. |
| `make sync` | Follow and incrementally download the latest job's results. |
| `make stop SLURMY_ID=JOB_ID` | Stop the whole Slurmy job and cancel its outstanding SLURM allocations. |
| `make resume SLURMY_ID=JOB_ID` | Resume only unfinished solver-problem pairs in a stopped job. |
| `make clean` | Remove generated submission files, leaving definitions and downloaded builds intact. |

Set `SLURMY_HOST` (default `datalab`) and `SLURMY_PARTITION` (default
`CPU-amd`) to choose the SSH alias and partition. For example:

```bash
make SLURMY_HOST=my-cluster SLURMY_PARTITION=my-cpu-partition prepare-submit
```

Changing an input file or the Makefile refreshes the prepared scripts on the
next `make`. For a one-off `BATCH_SIZE` override, run `make clean` first:
command-line variable changes do not alter file timestamps. `make submit`
always uses the current prepared files, so use `make prepare-submit` when you
want changed inputs incorporated. `make sync SLURMY_ID=JOB_ID` selects a
specific job rather than the latest one.

</blockquote>
</details>

The optional **SLURM reservation** beside the partition field accepts a code
such as `E192-04`. It is passed to both cluster build tasks and solver tasks,
and retained when a stopped job resumes. CLI workflows can set
`SLURMY_RESERVATION := E192-04` in their Makefile; leave it empty for normal scheduling.

To use only shared problems, add a cluster TPTP source without adding a local
source. **Browse cluster TPTP…** lists `/share/TPTP/` on the selected SSH host.
Open a version and its `Problems/` directory, then choose a file or select a
directory to form an editable recursive `/**/*.p` glob. For example,
`tptp://TPTP-v9.2.1/Problems/AGT/*.p` resolves to
`/share/TPTP/TPTP-v9.2.1/Problems/AGT/*.p`. A shared problem uses its version's
axiom library unless custom axioms or `SLURMY_TPTP_ROOT` override it.
