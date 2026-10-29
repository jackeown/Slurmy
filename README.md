<div align="center">
  <img src="implementation/logo.svg" alt="Slurmy logo" width="240">
</div>

# Slurmy

Slurmy is a web app that helps you run (multiple) solvers on (multiple) problems 
on a SLURM cluster, from your local computer.
Solvers can be provided ready for execution on the cluster, or can be built on the
cluster - your computer need not have the same architecture as the cluster.
Problems are provided as `glob`s of problem files on your computer, or as `.tgz`s
of problem files.
Slurmy runs all the solvers on all the problems using the SLURM cluster's nodes,
with resource limits and resource reporting.
Solvers' outputs can be viewed as they are produced, solvers' performances can be
compared in a graphical interface, and results can be downloaded incrementally to
your computer. 
The user does not need to use or understand SLURM commands or scripts.

**Slurmy terminology:**
The cluster management service is **SLURM** (all uppercase because it's an acronym).
A **workflow** defines the solvers, problems, resource limits, and SLURM parameters.
Each time a workflow is dispatched, a (new) **job** is created.
The individual **solver–problem pair**s in a job are divided into **batch**s.
Each batch is executed as one SLURM **task**.
The solver–problem pairs in a batch are executed sequentially in the task.
The resource usage of each solver–problem pair is monitored individually.
The results of a job can be downloaded in a `.tgz` file, with a summary in a`.csv`
file.

<details>
<summary><strong>🚀 Install and start</strong></summary>
<blockquote>
  
Your computer needs to have `python` (3.9+), `bash`, `ssh`, `rsync`, `tar`, and 
`make`, plus SSH access to a cluster that offers SLURM. Linux and macOS are supported; 
on Windows, use WSL. On macOS, `brew install python rsync` if they are missing.
Install macOS's command-line tools if `make` is missing. 
The macOS `bash` and `tar` are sufficient (no GNU tar replacement is required).

Solvers can be provided as binary executables, or Slurmy can build them on the cluster, i.e.,
the user's computer does not need to have the same CPU architecture as the cluster, but the
cluster needs to have the build tools required to build the solvers.

<details open>
<summary><strong>🏫 Cluster access</strong></summary>
<blockquote>

Slurmy relies on SSH access to a cluster that offers SLURM.
These setup notes are for TU Wien users; analogous processes
apply to other clusters.

TU Wien users must be on the campus network, via VPN if off campus.
The default SSH alias `datalab` refers to the dataLAB cluster.
Access is managed by dataLAB, not by Slurmy.
The [GPU Cluster guide](https://colab.tuwien.ac.at/spaces/Datalab/pages/241107279/GPU+Cluster#GPUCluster-LogintotheCluster)
has the current requirements and login instructions.
In brief:
1. Complete the **dataLAB Cluster Essentials** TUWEL course linked from the guide.
2. Create a [dataLAB account](https://login.datalab.tuwien.ac.at/) with your TU Wien credentials and add your SSH public key there.
Verify `ssh datalab` works. 
4. Request cluster login permission through the dataLAB [Matrix channel](https://matrix.to/#/#gpu:tuwien.ac.at) or the
[GPU SLURM Cluster Access request](https://jira.it.tuwien.ac.at/servicedesk/customer/portal/7/create/383).
The guide explains which affiliation confirmation to provide.
5. Once access is approved, add an SSH alias on your computer, replacing the username with your dataLAB account name:

```sshconfig
Host datalab
    HostName cluster.datalab.tuwien.ac.at
    User YOUR_DATALAB_USERNAME
    AddKeysToAgent yes
    IdentityFile ~/.ssh/YOUR_PRIVATE_KEY_FILE
```

Slurmy uses `datalab` by default. Set the environment variable `SLURMY_HOST` or choose another SSH host in the app
if your alias differs. 

</blockquote>
</details>

Install Slurmy like this:
```bash
python -m venv .venv          # Optional: keep dependencies separate from other projects.
source .venv/bin/activate     # Skip this line if you skipped the venv.
python -m pip install -r requirements.txt
```

Run Slurmy like this:
```
python web-app/main.py        # Opens the local app; Ctrl-C stops its server.
```
or like this:
```
python web-app/main.py        # Opens the local app; leaves the app running in the background
python web-app/main.py --stop # Stops the background app
```

The app runs in a web browser at `127.0.0.1:7569`.
Closing the browser tab does not stop Slurmy, and stopping the app does not stop SLURM
cluster jobs.
Slurmy is a single-user tool with access to your local files and trusted workflow commands;
do not expose it as a public web service.

</blockquote>
</details>

<details>
<summary><strong>🌐 Creating a workflow</strong></summary>

<blockquote>

Workflows are reusable definitions; a job is one particular run of a workflow.
The 
Open **New workflow** in the app, or duplicate one of the example workflows for under **Workflows**.
Saved workflows live in `workflows/my-workflows/`. 

**Give the workflow a home:**
Provide a name for the workflow, specify where it will run, and the ssh host alias.

**Specify the solver configurations:**
`Build solver configurations here × problem sources` is used to specify a new solver configurations.
The other options allow resuse of existing configurations.
Each **New Configuration** requires:
- A configuration name.
- A "root" directory on the local computer.
- The command to run the solver, using the available placeholder parameters as required.
- A selection where the solver gets built, either "No build" if a binary is available on the local computer,
or "Build from source" to build the solver on a cluster compute node (which ensures library compatibility,
etc.
In the latter case specify the source directory on the local computer, and a script in that directory that
is run to build the solver.
- Resource and SLURM parameters for each execution of the solver: wall clock limit, CPU limit, memory limit,
number of cores to allocate, number of threads to use per core, whether an entire CPU must be reserved for
the execution, and whether an entire cluster node must be reserved for the execution.

**Select the problems:**
The problems can come from the local computer, or from shared space on the cluster.
Problems are selected as globs of problem files and `.tgz` files (that conptain problem files).
For problems in the TPTP format, the source of `include` files, e.g., axiom files, are 
similarly selected as globs.

**Workflow settings:**
The tool used to monitor resource usage for one solver-problem pair execution is specified with
the available placeholder parameters.
The tool can also be built from source by specifying the source directory on the local computer,
and a script in that directory that is run to build the tool.

**SLURM settings:**
The solver-problem pairs can be run in problem major order, i.e., for each problm run all solvers,
or solution major order, i.e., for each solver run all problems, or random order.
The number of solver-problem pairs in each batch (that is run as one SLURM task) can be varied;
a suggested value is provided.
(Suggested explanation for review: By default, each Slurmy job may have at most 32 SLURM tasks pending or running simultaneously. As tasks finish, Slurmy submits more. This is a configurable submission window, not a limit on the total number of tasks or a fixed cluster limit. Concurrent Slurmy submissions under the same cluster account share available submission capacity.)

**Save the workflow settings:**
This generates all the files and scripts that get used to execute a job based on the workflow.
It does not submit the job to SLURM.

See [workflow structure and Makefile targets](workflows/README.md) for the files the
app creates and how to use a workflow from the command line.
See [implementation and CLI details](implementation/README.md) only if you want to
work below the web interface.

</blockquote>
</details>

<details>
<summary><strong>🏃‍♀️ Running a job</strong></summary>

<blockquote>

**Prepare & dispatch job** creates the submission files, builds declared
resources on the cluster, and submits the job. The arrow menu also offers
prepare-only and dispatch-already-prepared actions.

Use **Stop job** to cancel remaining SLURM cluster tasks, and **Resume job** to
run only unfinished tasks in the job.
An unfinished job must be stopped before deletion.

When deleting a job whose results have been downloaded to your computer, you can
keep the local data or remove it too.

**Cluster status** shows SLURM nodes, allocated physical cores, users with active
jobs, and running or pending tasks across users.
It defaults to **CPU-amd**. The pie chart, summary, nodes, users, and tasks all
follow the selected partition; **All partitions** includes the whole cluster.
The cores-by-user chart has a **Percentages relative to** selector:
**Allocated cores** compares users' shares of current allocations; **Total cores**
compares them against all physical cores in the selected partition and includes
an **Unallocated cores** slice. Unallocated cores can include offline nodes,
so the slice is not a guarantee of schedulable capacity.
The selected view is retained in the page URL.
Select a node to see the tasks currently running on it.
A running task's row shows its node and SLURM ID.
For new Slurmy submissions, it also shows solver-problem pair WHAT?? and batch counts across users.
Jobs whose private files you can read link to their job inspector WHAT??; older users' jobs
might not expose counts.

</blockquote>
</details>

<details>
<summary><strong>📖 Looking at job results</strong></summary>

<blockquote>

In **Job history**, follow progress and open a job to inspect solver-problem pair outcomes,
solver comparisons, problems, output streams, and diagnostics. Select solver-problem pair
outcomes to filter the solver-problem pairs table by multiple statuses; the 3D plot compares outcome
counts across solvers. **Download** saves three files under
`job-results/JOB_ID/`: `<job-name>_results-summary.csv`,
`<job-name>_solver-stdout.tgz`, and `<job-name>_solver-stderr.tgz`.
Each archive extracts into its own directory, with one log per solver-problem pair named
`<solver>___<original-problem-filename>___<solver-problem-pair-id>.log` (or `.stderr.log`
for stderr). Problem filenames, including their extensions and characters such
as `+` and `^`, are preserved exactly. Unsafe filename characters in the job
name are replaced with hyphens. The arrow menu offers
**Complete/verbose download**
under `job-results/verbose/JOB_ID/`. Readable individual archives are grouped in
`solver-problem-pairs/` and named
`<solver>___<original-problem-filename>___<solver-problem-pair-id>.tgz`.
Each extracts into a matching directory containing solver logs and diagnostic
files. The `diagnostics/` directory preserves the original cluster files,
including raw archives and retry history, SLURM logs, plans, and metadata.
Consolidated stdout/stderr archives and download statistics remain at the top
level. `CONTENTS.txt` explains the layout. Existing verbose downloads are
organized into this layout on their next download.

The workflow editor suggests a batch size when you enter **SLURM settings**:
`ceil(total solver-problem pairs / (2 × maximum parallelism))`, with a minimum
of 1. It queries the selected partition's current node topology and counts the
selected problem sources. Shared tasks fit by requested physical cores;
dedicated CPU tasks consume whole sockets, and dedicated-node tasks consume a
whole node. The estimate uses the least parallel solver configuration and is
capped by the workflow's maximum outstanding SLURM tasks. It measures total
CPU capacity rather than idle capacity; memory and other scheduling constraints
can reduce actual parallelism. Saved values and manual overrides are retained;
**Use suggested batch size** applies the freshly calculated value.

</blockquote>
</details>

<details>
<summary><strong>🗂️ Repository map</strong></summary>

<blockquote>

- [`workflows/`](workflows/README.md) — your saved workflows, plus some example
  problems, solvers, and workflows, and your saved workflows.
- [`job-results/`](job-results/README.md) — downloaded job results (created as needed).
- [`web-app/`](web-app/) — local browser interface. Not really of interest
  to someone who wants to run jobs on the cluster, but interesting if you want
  to see how the interface magic happens.
- [`implementation/`](implementation/README.md) — job runner, submission templates,
  build tooling, and optional command-line utilities. Not really of interest
  to someone who wants to run jobs on the cluster, but interesting if you want
  to see how the execution-with-SLURM magic happens.

</blockquote>
</details>

Job history shows each job's name and a combined status/progress bar. Hover a
segment for its solver outcome, count and percentage; striped segments mark
unfinished solver-problem pairs. Execution failures are outcomes rather than a
separate “Issues” column. The job page links to its originating workflow, and
hovering its title reveals the full ID.

When no custom job name is supplied, Slurmy uses the workflow name, adding
`(1)`, `(2)`, etc. only when that name is already present on the cluster. Name
assignment is serialized across submissions. A submission retry retains its
assigned name. Names are stored alongside stable filename-safe IDs in job
metadata; names never need to be decoded from filenames. Existing job IDs and
custom job names remain valid.

The palette menu beside the GitHub link in the sidebar offers 16 preset themes:
four everyday light palettes, four everyday dark palettes, four atmospheric
dark themes, and four patterned light themes. Hover an option (or focus it
with the keyboard) to preview the whole app; click to save it in this browser.
Leaving the menu or dismissing it restores the saved theme. Neon Haze adds a
subtle grid, cyan bloom, and violet fog; its movement respects reduced-motion
preferences. Existing light/dark preferences map to Porcelain/Evergreen.

Preset themes also vary typography, corner radius, surface transparency,
transitions, shadows, and ambient motion. Decorative cursor and hover effects
are disabled, including in previously saved or uploaded themes.
Amethyst uses one seamless, page-wide ambient loop.

**Custom Theme** opens a live editor for palette colors, installed fonts,
corner radius, glass opacity/blur, patterns,
transition duration, background motion, box shadows, and heading text shadows.
You can choose a local PNG/JPEG/WebP/GIF background (up to 20 MB), its fit, and
opacity. Large images are resized/compressed locally before preview and saving. Every control previews immediately. **Save Custom Theme** stores the
settings and image in this browser; **Discard Changes** or Escape restores the
saved theme. A storage-quota error keeps the editor open so the image can be
removed. Reduced-motion preferences disable CSS motion effects.

Job History loads compact per-job summaries rather than individual progress,
manifest, and log files. Summaries are cached on the cluster in
`~/.cache/slurmy/history-v2` and reused across local app restarts. Atomic result
publications, index updates, metadata changes, and stop/resume submission records
invalidate the affected job's cache. An hourly safety refresh catches external
in-place edits. SLURM queue/accounting status is queried fresh on each collection.
The first scan of an older job may take longer while its cache is populated;
SSH keepalives detect lost connections during that scan. Cache files are disposable
and do not modify results or workflow files.

SSH requests throughout the app, submission scripts, build driver and downloads
reuse private OpenSSH control sockets. Idle connections expire after 60 seconds;
keepalives detect broken transports. Socket directories must belong to the current
user and have mode 0700; otherwise connection sharing is disabled. No changes to
`~/.ssh/config` are required. Multiple TPTP globs are resolved in one remote query,
with fresh matches on each preparation.

Prepared workflows retain readable per-pair scripts locally. Uploads contain one
compressed script bundle per batch, rather than thousands of small script files.
Each compute node unpacks its batch into node-local temporary storage, removed at
batch exit. Upload commit and SLURM dispatch share one SSH channel. Prebuilt-only
jobs perform no cluster builds, and `/share/TPTP` problems are never uploaded.
SLURM queue waits, solver runtime and genuinely large resource transfers still
contribute to time to first result.
