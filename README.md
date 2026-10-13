<div align="center">
  <img src="implementation/logo.svg" alt="Slurmy logo" width="240">

# Slurmy

</div>

Slurmy is a web app that helps you run (multiple) solvers on (multiple) problems 
on a Slurm cluster, from your own computer. Solver builds and execution happen on 
the cluster, so your computer and the cluster need not have the samevarchitecture. 
You can inspect jobs, compare solver outcomes, read all output,vand incrementally 
download results without working directly with Slurm scripts.

A **workflow** defines the solvers, problems, resource
limits, and Slurm cluster parameters. Each time you dispatch a workflow, Slurmy
creates a **job**: the individual solver–problem execution are grouped into Slurm
**batch**s. The resource usage of each solver–problem run is monitored separately.
The results of a job can be downloaded in a `.tgz` file, with a summary in a`.csv` file.

<details>
<summary><strong>🚀 Install and start</strong></summary>

<blockquote>

Your computer needs to have `python` (3.9+), `bash`, `ssh`, `rsync`, `tar`, and 
`make`, plus SSH access to a supported Slurm cluster. Linux and macOS are supported; 
on Windows, use WSL. On macOS, `brew install python rsync` supplies `python` and 
`rsync` if they are missing. Install Apple's command-line tools if `make` is missing. 
The system `bash` and `tar` are sufficient (no GNU tar replacement is required).
The cluster needs the build tools required by your chosen solvers - you do not need 
to compile solvers locally.

After cloning the project, install Slurmy like this:
```bash
python -m venv .venv          # Optional: keep dependencies separate from other projects.
source .venv/bin/activate     # Skip this line if you skipped the venv.
python -m pip install -r requirements.txt
```
After installation, run Slurmy like this:
```
python web-app/main.py        # Opens the local app; Ctrl-C stops its server.
```
or like this:
```
python web-app/main.py        # Opens the local app; leaves the app running in the background
python web-app/main.py --stop # Stops the background app
```

The app runs on your computer at `127.0.0.1:8765` by default. It is a
single-user tool with access to your local files and trusted workflow commands;
do not expose it as a public web service. Closing the browser tab does not stop 
Slurmy, and stopping the app does not stop cluster jobs.

</blockquote>
</details>

<details>
<summary><strong>🌐 Create a workflow and inspect its jobs</strong></summary>

<blockquote>

Open **New workflow** in the app, or start with an example under **Workflows**.
The guided form covers solver commands and builds, problems and optional TPTP
axioms, the resource limiter, and Slurm settings. Runsolver is the default
limiter. Saving a workflow creates its definition locally; it does not submit
a job. **Prepare & dispatch job** creates the submission files, builds declared
resources on the cluster, and submits the job. The arrow menu also offers
prepare-only and dispatch-already-prepared actions.

In **Job history**, follow progress and open a job to inspect call outcomes,
solver comparisons, problems, output streams, and diagnostics. Select a call
outcomes to filter the calls table by multiple statuses; the 3D plot compares outcome
counts across solvers. **Sync/Download** saves exactly three files under
`job-results/JOB_ID/`: `calls.csv`, `solver-logs.tgz`, and
`solver-errors.tgz`. The arrow menu offers **Complete/verbose sync/download**
for raw per-call archives, Slurm logs, plans, and metadata under
`job-results/verbose/JOB_ID/`.
The workflow editor keeps Slurm settings separate from Review; generating a
potentially expensive preview is an explicit action. When
deleting a synced job, you can keep its local copy or remove it too; an
incomplete last download can be synced first.
Workflows are reusable definitions; jobs are particular runs of them.

**Cluster status** shows Slurm nodes, CPU allocation, users with active jobs,
and running or pending jobs across users. It starts with the `CPU-amd`
partition; choose another partition or all partitions from the page. Select a
node to see the jobs currently running on it. For
new Slurmy submissions, it also shows call and batch counts across users via
Slurm's queue metadata. Jobs whose private files you can read link to their
job inspector; older users' jobs may not expose counts.

Example workflows for Vampire, E, and Drodi are read-only; duplicate one to
edit it. Your saved workflows live in `workflows/my-workflows/`. See
[workflow structure and Makefile targets](workflows/README.md) for the files the
app creates and how to use a workflow from the command line. See
[implementation and CLI details](implementation/README.md) only if you want to
work below the web interface.

</blockquote>
</details>

<details>
<summary><strong>🏫 TU Wien dataLAB cluster access</strong></summary>

  
<blockquote>
For TU Wien users, the default SSH alias `datalab` refers to the dataLAB
cluster. Access is managed by dataLAB, not by Slurmy. Their
[GPU Cluster guide](https://colab.tuwien.ac.at/spaces/Datalab/pages/241107279/GPU+Cluster#GPUCluster-LogintotheCluster)
has the current requirements and login instructions. In brief:

1. Complete the **dataLAB Cluster Essentials** TUWEL course linked from the
   guide.
2. Create a [dataLAB account](https://login.datalab.tuwien.ac.at/) with your
   TU Wien credentials and add your SSH public key there.
3. Request cluster login permission through the dataLAB
   [Matrix channel](https://matrix.to/#/#gpu:tuwien.ac.at) or the
   [GPU Slurm Cluster Access request](https://jira.it.tuwien.ac.at/servicedesk/customer/portal/7/create/383).
   The guide explains which affiliation confirmation to provide.
4. Once access is approved, add an SSH alias on your computer, replacing the
   username with your dataLAB account name:

```sshconfig
Host datalab
    HostName cluster.datalab.tuwien.ac.at
    User YOUR_DATALAB_USERNAME
    AddKeysToAgent yes
    IdentityFile ~/.ssh/YOUR_PRIVATE_KEY_FILE
```

Then verify `ssh datalab` works before submitting a workflow. Slurmy uses
`datalab` by default; set `SLURMY_HOST` or choose another SSH host in the app
if your alias differs. Choose a partition available to your account in the
workflow settings. If you are off campus, set up VPN
access to the TU Wien network to reach the cluster.

</blockquote>
</details>

<details>
<summary><strong>🗂️ Repository map</strong></summary>

<blockquote>

- [`web-app/`](web-app/) — local browser interface.
- [`workflows/`](workflows/README.md) — example problems and solver sources,
  example workflows, and your saved workflows.
- [`implementation/`](implementation/README.md) — runner, submission templates,
  build tooling, and optional command-line utilities.
- `job-results/` — locally synced results (created as needed).

</blockquote>
</details>
