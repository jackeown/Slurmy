<div align="center">
  <img src="implementation/logo.svg" alt="Slurmy logo" width="240">

# Slurmy

</div>

Slurmy helps you run theorem-proving experiments on a Slurm cluster from your
own computer. Define a reusable **workflow** with solvers, problems, resource
limits, and optional cluster-side builds. Each time you dispatch it, Slurmy
creates a **job**: the individual solver–problem calls are grouped into Slurm
tasks, and their results can be explored in a local web app.

The web app is the intended starting point. Solver builds and proving happen
on the cluster, so your laptop and the cluster need not have the same
architecture. You can inspect jobs, compare solver outcomes, read call output,
and incrementally download results without working directly with Slurm scripts.

<details>
<summary><strong>🚀 Install and start</strong></summary>

<blockquote>

You need Python 3.9+, Bash, SSH, rsync, tar, and make on your computer, plus
SSH access to a supported Slurm cluster. Linux and macOS are supported; on
Windows, use WSL. The cluster needs the build tools required by your chosen
solvers, but you do not need to compile those solvers locally.

From the repository root:

```bash
python -m venv .venv          # Optional: keep dependencies separate from other projects.
source .venv/bin/activate    # Skip this line if you skipped the venv.
python -m pip install -r requirements.txt
python web-app/main.py       # Opens the local app; Ctrl-C stops its server.
```

On macOS, `brew install python rsync` supplies Python and rsync if they are
missing. Install Apple's command-line tools if `make` is missing. The system
Bash and tar are sufficient; no GNU tar replacement is required.

The app runs on your computer at `127.0.0.1:8765` by default. It is a
single-user tool with access to your local files and trusted workflow commands;
do not expose it as a public web service. Use `python web-app/main.py --stop`
to stop a background server. Closing the browser tab does not stop it, and
stopping the app does not cancel cluster jobs.

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
solver comparisons, problems, output streams, and diagnostics. **Sync/Download**
copies new results to `job-results/` without downloading unchanged files.
Workflows are reusable definitions; jobs are particular runs of them.

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
    IdentityFile ~/.ssh/YOUR_PRIVATE_KEY
```

Then verify `ssh datalab` works before submitting a workflow. Slurmy uses
`datalab` by default; set `SLURMY_HOST` or choose another SSH host in the app
if your alias differs. Choose a partition available to your account in the
workflow settings. If you are off campus, arrange the TU Wien network or VPN
access needed to reach the cluster.

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
