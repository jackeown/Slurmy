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
A **workflow** defines the solvers, problems, resource limits, and SLURM cluster 
parameters.
Each time a workflow is dispatched, a (new) **job** is created.
The individual **solver–problem pair**s in a job are grouped into **batch**s.
Each batch is executed as one SLURM **task**.
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
[GPU Slurm Cluster Access request](https://jira.it.tuwien.ac.at/servicedesk/customer/portal/7/create/383).
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

The app runs on in a web browser at `127.0.0.1:8765`.
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
An incomplete last download can be synced first. WHAT???

**Cluster status** shows SLURM nodes, reserved physical cores, users with active 
jobs, and running or pending tasks across users.
It defaults to the `CPU-amd` cluster; choose another cluster or all clusters from 
the page.
Select a node to see the tasks currently running on it.
A running task's row shows its node and SLURM ID.
For new Slurmy submissions, it also shows call WHAT?? and batch counts across users.
Jobs whose private files you can read link to their job inspector WHAT??; older users' jobs
might not expose counts.

</blockquote>
</details>

<details>
<summary><strong>📖 Looking at job results</strong></summary>

<blockquote>

In **Job history**, follow progress and open a job to inspect call outcomes,
solver comparisons, problems, output streams, and diagnostics. Select call
outcomes to filter the calls table by multiple statuses; the 3D plot compares outcome
counts across solvers. **Sync/Download** saves three files under
`job-results/JOB_ID/`: `<job-name>_results-summary.csv`,
`<job-name>_solver-stdout.tgz`, and `<job-name>_solver-stderr.tgz`.
Each archive extracts into its own directory, with one log per call named
`<solver>___<original-problem-filename>___<jobpair-id>.log` (or `.stderr.log`
for stderr). Problem filenames, including their extensions and characters such
as `+` and `^`, are preserved exactly. Unsafe filename characters in the job
name are replaced with hyphens. The arrow menu offers
**Complete/verbose sync/download**
for raw per-call archives, Slurm logs, plans, and metadata under
`job-results/verbose/JOB_ID/`.

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
  to see how the execution-with-Slurm magic happens.

</blockquote>
</details>
