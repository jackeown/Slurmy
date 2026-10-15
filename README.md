<div align="center">
  <img src="implementation/logo.svg" alt="Slurmy logo" width="240">
</div>

# Slurmy

Slurmy is a web app that helps you run (multiple) solvers on (multiple) problems 
on a Slurm cluster, from your own computer. Solvers can be provided ready for 
execution on the cluster, or can be built on the cluster - your computer need 
not have the same architecture as the cluster. Problems are provided as
local `glob`s of problem files, or as `.tgz`s of problem files. Slurmy
executes a job that runs all the solvers on all the problems with resource limits
and resource reporting. Solvers' outputs can be viewed as they are produced, 
solvers' performances can be seen in a grphical interface, and results can be
downloaded incrementally. 
The user does not need to use or understand Slurm commands or scripts..

A **workflow** defines the solvers, problems, resource limits, and Slurm cluster 
parameters. Each time a workflow is dispatched, a (new) **job** is created. The
individual solver–problem pairs in a job are grouped into **batch**s. Each batch is
executed as one **job-step**. The resource usage of each solver–problem pair is
monitored individually. The results of a job can be downloaded in a `.tgz` file,
with a summary in a`.csv` file.

<details>
<summary><strong>🚀 Install and start</strong></summary>

Your computer needs to have `python` (3.9+), `bash`, `ssh`, `rsync`, `tar`, and 
`make`, plus SSH access to a cluster that offers Slurm. Linux and macOS are supported; 
on Windows, use WSL. On macOS, `brew install python rsync` if they are missing.
Install macOS's command-line tools if `make` is missing. 
The macOS `bash` and `tar` are sufficient (no GNU tar replacement is required).

Solvers can be provided as binary executables, or Slurmy can build them on the cluster, i.e.,
the user's computer does not need to have the same CPU architecture as the cluster, but the
cluster needs to have the build tools required to build the solvers.

<details open>
<summary><strong>🏫 Cluster access</strong></summary>

Slurmy relies on SSH access to a cluster that offers Slurm.
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
Closing the browser tab does not stop Slurmy, and stopping the app does not stop cluster jobs.
Slurmy is a single-user tool with access to your local files and trusted workflow commands;
do not expose it as a public web service.

</details>

<details>
<summary><strong>🌐 Creating a workflow</strong></summary>

<blockquote>

Workflows are reusable definitions; a job is one particular run of a workflow.
The example workflows for Vampire, E, and Drodi are read-only; duplicate one to
edit it.
Saved workflows live in `workflows/my-workflows/`. 

Open **New workflow** in the app, or start with an example under **Workflows**.
The guided form covers cluster specification, solver commands and builds, local or shared cluster
problems, optional TPTP axioms, the resource limiter (`runsolver` is the default), and Slurm settings.
Saving a workflow creates and saves its definition locally; it does not submit a job.

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

Use **Stop job** to cancel
remaining Slurm work and **Resume job** to run only unfinished calls in the
same job. An unfinished job must be stopped before deletion. When deleting a
synced job, you can keep its local copy or remove it too; an incomplete last
download can be synced first.

**Cluster status** shows Slurm nodes, CPU allocation, users with active jobs,
and running or pending jobs across users. It starts with the `CPU-amd`
partition; choose another partition or all partitions from the page. Select a
node to see the jobs currently running on it. A running job's row shows its
node and Slurm ID. For new Slurmy submissions, it also shows call and batch
counts across users. Jobs whose private files you can read link to their
job inspector; older users' jobs might not expose counts.

</blockquote>
</details>

<details>
<summary><strong>📖 Looking at job results</strong></summary>

<blockquote>

In **Job history**, follow progress and open a job to inspect call outcomes,
solver comparisons, problems, output streams, and diagnostics. Select call
outcomes to filter the calls table by multiple statuses; the 3D plot compares outcome
counts across solvers. **Sync/Download** saves three files under
`job-results/JOB_ID/`: `<job-name>.csv`, `<job-name>_logs.tgz`, and
`<job-name>_errors.tgz`. Each archive extracts into its own directory, with
one descriptively named log per call. Unsafe filename characters in the job
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
