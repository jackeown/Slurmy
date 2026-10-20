# Slurmy: from workflow creation to solver-problem pair execution

This diagram follows **creating a workflow in the UI → Prepare & dispatch
job → executing one solver-problem pair**.

There are three execution locations:

- **Your computer:** browser, local web server, workflow files, and submission tools.
- **Cluster login/head node:** receives files and calls `sbatch`.
- **Cluster compute nodes:** build resources and execute solver calls.

## Full execution chain

```text
YOUR COMPUTER
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 1. Create a workflow in the browser                                       │
  │                                                                          │
  │ Select:                                                                  │
  │   • Solver configurations and commands                                   │
  │   • Solver runtime directories and optional build recipes                 │
  │   • Problems: files, globs, archives, or tptp://TPTP-v9.2.1/Problems/*/*.p              │
  │   • Default/shared TPTP axioms or custom axioms                            │
  │   • Per-call CPU, wall-time, memory, core and thread limits                │
  │   • Resource limiter, normally runsolver                                  │
  │   • SSH host, Slurm partition, batch size, submission window               │
  │                                                                          │
  │ UI: templates/new.html + static/builder.js                                │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ HTTP POST /api/workflows
                                     │ Workflow choices sent as JSON
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 2. Local Flask server validates the workflow                              │
  │                                                                          │
  │ app.py → workflows.save() → workflows.specification()                     │
  │                                                                          │
  │   • Validate paths, solver settings, resource declarations and commands    │
  │   • Query cluster topology over SSH                                      │
  │   • Expand problem selections                                            │
  │   • Construct explicit solver–problem rows                               │
  │   • Use slurmy.generate() in a temporary directory to validate the plan   │
  │                                                                          │
  │ Shared TPTP problem selections are listed over SSH.                       │
  │ Those problem files remain on the cluster.                               │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 3. Save the reusable workflow definition                                  │
  │                                                                          │
  │ workflows/my-workflows/WORKFLOW_NAME/                                     │
  │   │                                                                      │
  │   ├── .slurmy-workflow.json        Editor choices                         │
  │   ├── Makefile                    Cluster settings and shared targets     │
  │   ├── jobpairs.csv                One explicit solver–problem call/row    │
  │   ├── building.txt                Resource roots and build recipes        │
  │   ├── resource_limiter_template.txt                                       │
  │   │                                                                      │
  │   ├── configurations.csv          For all-pairs workflows                 │
  │   ├── problem-globs.txt           For all-pairs workflows                 │
  │   ├── axiom-globs.txt             If using custom axioms                  │
  │   └── build-resource-*.sh          If build commands were entered inline  │
  │                                                                          │
  │ Saving defines the workflow. It does not submit a job.                    │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ User clicks “Prepare & dispatch job”
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 4. Start a local background operation                                    │
  │                                                                          │
  │ POST /api/workflow/action                                                │
  │   → app.py: experiment_action()                                          │
  │   → app.py: operation()                                                  │
  │   → subprocess: make prepare-submit                                     │
  │                                                                          │
  │ The web server records the operation log under web-app/.state/.           │
  │ The browser polls the operation API to display progress.                 │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 5. Prepare submission files                                              │
  │                                                                          │
  │ Workflow Makefile includes implementation/templates/workflow.mk           │
  │                                                                          │
  │ make prepare-submit                                                     │
  │   └── make prepare                                                      │
  │         ├── Refresh jobpairs.csv for all-pairs workflows                  │
  │         │     └── implementation/slurmy-pairs.py                          │
  │         ├── Check partition threads/core over SSH                        │
  │         │     └── implementation/check_threads.py                         │
  │         └── Generate or refresh the prepared submission                   │
  │               └── implementation/slurmy.py                               │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 6. slurmy.py compiles the workflow into an execution plan                  │
  │                                                                          │
  │   • Read jobpairs.csv, building.txt and limiter template                  │
  │   • Assign task IDs and batch IDs                                        │
  │   • Render commands and relocate packaged paths                          │
  │   • Group calls into sequential batches                                  │
  │   • Give node-exclusive calls their own batches                          │
  │                                                                          │
  │ Generated locally:                                                      │
  │                                                                          │
  │ WORKFLOW_NAME/                                                          │
  │   ├── submit.sh                                                         │
  │   └── submit.sh.files/                                                  │
  │         ├── metadata.json              Job counts and batch mapping       │
  │         ├── manifest.jsonl             Definitions of all calls           │
  │         ├── archive-paths.txt          Inputs to package                   │
  │         ├── submission_settings.sh                                        │
  │         ├── remote_prepare.sh                                             │
  │         ├── remote_finish_upload.sh                                       │
  │         ├── remote_submit.sh                                              │
  │         ├── submit_coordinator.py                                         │
  │         ├── batch.sh                                                      │
  │         ├── call.sh                                                       │
  │         ├── timed_call.sh                                                 │
  │         ├── csv.sh                                                        │
  │         ├── builds/                                                       │
  │         │     ├── run.sh                Launch declared builds            │
  │         │     └── driver.py             Copy of slurmy-build.py           │
  │         ├── plans/                                                        │
  │         │     ├── batch_000000.sh       Call IDs and resource requirements │
  │         │     └── ...                                                     │
  │         └── calls/                                                        │
  │               ├── 0/                                                      │
  │               │   ├── config.sh        Limits, paths, IDs                 │
  │               │   ├── limiter.sh       Rendered limiter command           │
  │               │   └── solver.sh        Rendered solver command            │
  │               └── ...                                                     │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ make submit → bash ./submit.sh
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 7. Run submit.sh locally                                                 │
  │                                                                          │
  │   • Recheck requested threads/core against the selected partition         │
  │   • Run builds/run.sh                                                    │
  │                                                                          │
  │ For each resource with a declared build recipe:                          │
  │   → python builds/driver.py ...                                          │
  │   → Upload source/context, recipe.sh and generated job.sh                │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ SSH / file transfer
                                     ▼

CLUSTER LOGIN / HEAD NODE — BUILD PHASE
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 8. Publish the build directory and submit the build                        │
  │                                                                          │
  │ ~/slurmy/builds/BUILD_ID/                                                 │
  │   ├── context.tar.gz                                                    │
  │   ├── recipe.sh                                                         │
  │   └── job.sh                                                            │
  │                                                                          │
  │ cd ~/slurmy/builds/BUILD_ID                                              │
  │ sbatch --parsable job.sh                                                 │
  │                                                                          │
  │ Implemented in implementation/build/slurmy-build.py                       │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ Slurm queues and allocates the build
                                     ▼

CLUSTER COMPUTE NODE — BUILD PHASE
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 9. Slurm executes job.sh                                                 │
  │                                                                          │
  │   • Extract source into the build working directory                       │
  │   • Run: bash "$SLURMY_BUILD_ROOT/recipe.sh"                              │
  │   • Check the expected output artifact                                   │
  │                                                                          │
  │ Output under ~/slurmy/builds/BUILD_ID/output/                             │
  │                                                                          │
  │ No srun is inserted here.                                                │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ Build driver monitors completion,
                                     │ then downloads the output
                                     ▼

YOUR COMPUTER — PACKAGE AND DISPATCH
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 10. Package the job after builds finish                                  │
  │                                                                          │
  │ Downloaded build outputs populate the local runtime roots.               │
  │                                                                          │
  │ submit.sh → package_inputs.py                                            │
  │                                                                          │
  │   inputs.tar.gz                                                         │
  │     Solver/runtime directories, limiter, local problems, custom axioms   │
  │                                                                          │
  │   job-files.tar.gz                                                      │
  │     Contents of submit.sh.files/                                        │
  │                                                                          │
  │ Shared cluster TPTP problems are referenced, not uploaded.                │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ SSH: remote_prepare.sh
                                     │ SCP: upload the two archives
                                     │ SSH: remote_finish_upload.sh
                                     ▼

CLUSTER LOGIN / HEAD NODE — SOLVER-JOB SUBMISSION
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 11. Stage and publish the job inputs                                     │
  │                                                                          │
  │ Upload into a private staging directory:                                 │
  │   ~/slurmy/jobs/.staging-JOB_ID/                                         │
  │                                                                          │
  │ After the upload completes, publish as:                                  │
  │   ~/slurmy/jobs/JOB_ID/                                                  │
  │                                                                          │
  │ Inputs are unpacked into rootfs/, preserving their local path layout.    │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ submit.sh invokes over SSH:
                                     │ bash -s ... < remote_submit.sh
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 12. remote_submit.sh resolves the actual Slurm allocations                │
  │                                                                          │
  │ Working directory: ~/slurmy/jobs/JOB_ID/                                 │
  │                                                                          │
  │   • Hold this job’s submission-controller lock                           │
  │   • Validate Slurm core-allocation / cgroup configuration                 │
  │   • Inspect the partition’s socket/core/thread topology                   │
  │   • Resolve automatic socket requirements                                │
  │   • Check every planned batch fits the nodes                             │
  │   • Generate batches/batch_XXXXXX.sh and allocations.csv                  │
  │                                                                          │
  │ For each batch, construct sbatch options:                                │
  │   partition, cores/threads, memory, time, exclusivity, output path, ...    │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ For each batch:
                                     │
                                     │ python3 submit_coordinator.py \
                                     │   attempt JOB_ID MAX_OUTSTANDING \
                                     │   -- [Slurm options] batch.sh
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 13. submit_coordinator.py controls submission capacity                    │
  │                                                                          │
  │   • Acquire ~/slurmy/submit.lock                                         │
  │   • Inspect the user’s current Slurm queue                               │
  │   • Account for other active Slurmy submitters                            │
  │   • Respect the per-job outstanding window and known submission limits   │
  │                                                                          │
  │ No capacity? → return WAIT → remote_submit.sh sleeps and retries          │
  │                                                                          │
  │ Capacity available?                                                      │
  │   → subprocess: sbatch [Slurm options] batch.sh                           │
  │   → return ACCEPTED with the Slurm job ID                                │
  │                                                                          │
  │ remote_submit.sh checkpoints accepted batches in submission.csv.         │
  │ Submission continues as capacity becomes available.                      │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ Slurm queues the batch
                                     │ and assigns an allocation
                                     ▼

CLUSTER COMPUTE NODE — EXECUTE A BATCH
══════════════════════════════════════════════════════════════════════════════════

  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 14. Slurm starts batch.sh                                                │
  │                                                                          │
  │ JOB_DIR = SLURM_SUBMIT_DIR = ~/slurmy/jobs/JOB_ID                         │
  │ BATCH_ID = exported SLURMY_BATCH_ID                                      │
  │                                                                          │
  │ Load: batches/batch_XXXXXX.sh                                            │
  │                                                                          │
  │   • Read the allocation’s allowed CPU set                                │
  │   • Inspect physical cores, sockets and sibling threads with lscpu        │
  │   • Choose cores satisfying the call’s placement requirements             │
  │   • Skip calls whose completed results are already saved                 │
  │                                                                          │
  │ Calls within this batch execute ONE AT A TIME.                           │
  │ Other batches can execute concurrently in other allocations.             │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ For one unfinished solver-problem pair:
                                     │
                                     │ taskset -c CPU_MASK \
                                     │   bash "$JOB_DIR/call.sh" TASK_ID BATCH_ID
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 15. call.sh sets up one solver-problem pair                                          │
  │                                                                          │
  │ Load calls/TASK_ID/config.sh                                             │
  │                                                                          │
  │   • Set TPTP to the shared or custom axiom root                           │
  │   • Set OMP_NUM_THREADS = cores × threads_per_core                        │
  │   • Create a temporary per-call working directory                        │
  │   • Assign output/log paths                                              │
  │   • Atomically publish “running” progress                                │
  │   • cd to the packaged solver runtime directory                          │
  │                                                                          │
  │ Start a separate process session and an outer watchdog:                  │
  │                                                                          │
  │ setsid timeout --signal=TERM --kill-after=5 "$((WC_LIMIT + 25))" \         │
  │   bash "$JOB_DIR/timed_call.sh"                                          │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 16. timed_call.sh measures the whole limiter invocation                   │
  │                                                                          │
  │ Bash built-in time:                                                     │
  │   time bash "$CALL_DIR/limiter.sh"                                       │
  │                                                                          │
  │ Timing and controller diagnostics go to separate files.                  │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 17. limiter.sh executes the rendered resource-limiter command             │
  │                                                                          │
  │ Normally:                                                               │
  │                                                                          │
  │ runsolver                                                               │
  │   --cpu-limit CPU_LIMIT                                                 │
  │   --wall-clock-limit WALL_LIMIT                                         │
  │   --rss-swap-limit MEMORY_LIMIT_MIB                                      │
  │   --watcher-data WATCHER_LOG                                            │
  │   --var VAR_FILE                                                        │
  │   --solver-data SOLVER_LOG                                              │
  │   ...                                                                   │
  │   /bin/bash "$CALL_DIR/solver.sh"                                        │
  │                                                                          │
  │ The limiter enforces limits and measures the solver process tree.        │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 18. solver.sh executes the actual solver command                          │
  │                                                                          │
  │ For example:                                                            │
  │                                                                          │
  │ ./vampire --mode casc /share/TPTP/TPTP-v9.2.1/Problems/AGT/EXAMPLE.p     │
  │                                                                          │
  │ Or a problem path under "$JOB_DIR/rootfs/..." for an uploaded problem.    │
  │                                                                          │
  │ Solver stderr is captured separately from solver stdout.                 │
  │ CPU affinity is inherited from taskset.                                  │
  │                                                                          │
  │ THIS IS THE EXECUTION OF ONE JOBPAIR.                                    │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     │ Solver finishes, reaches a limit,
                                     │ errors, or is interrupted
                                     ▼
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ 19. call.sh interprets and publishes the result                           │
  │                                                                          │
  │   • Read exit status, timing, runsolver variables and watcher output      │
  │   • Extract SZS status and SZS output type                               │
  │   • Classify success, time/memory limit, solver error, interruption, ...  │
  │   • Save an archive of the call’s output and diagnostics                  │
  │   • Atomically publish the per-call result CSV                           │
  │   • Update the compact per-batch result index                            │
  │   • Publish final progress                                               │
  │                                                                          │
  │ ~/slurmy/jobs/JOB_ID/                                                    │
  │   ├── results/                                                          │
  │   │     ├── batch_..._task_....csv                                       │
  │   │     ├── batch_..._task_....tar.gz                                    │
  │   │     └── index/batch_XXXXXX.csv                                      │
  │   └── progress/                                                         │
  │         ├── task_TASK_ID.csv                                            │
  │         └── batch_XXXXXX.csv                                            │
  └──────────────────────────────────┬─────────────────────────────────────────┘
                                     │
                                     ├── batch.sh advances to the next call
                                     │
                                     └── After the last call, batch.sh exits
                                         Slurm records the allocation’s end


MONITORING BACK TO YOUR BROWSER
══════════════════════════════════════════════════════════════════════════════════

  Browser periodically requests job state
       │
       ▼
  Local Flask server: app.py
       │
       ▼
  monitor.py → RemoteCollector → SSH read-only snapshot
       │
       ├── Read metadata, manifests, progress and result indexes
       ├── Query squeue / sacct
       └── Fetch a selected call’s saved output when requested
       │
       ▼
  Parse and summarize the remote state locally
       │
       ▼
  Browser displays:
    progress • call outcomes • timings • memory • node/Slurm ID • output
```

The build phase is skipped for resources that have no build recipe; their
existing runtime directories are packaged as-is. Build context archives are
optional when a recipe obtains its own source.

## Process chain on the compute node

Stripped of preparation and monitoring, one solver-problem pair follows this chain:

```text
Slurm starts batch.sh
    │
    └── taskset -c CPU_MASK bash call.sh TASK_ID BATCH_ID
            │
            └── setsid timeout ... bash timed_call.sh
                    │
                    └── time bash limiter.sh
                            │
                            └── runsolver [limits and logging options]
                                    │
                                    └── /bin/bash solver.sh
                                            │
                                            └── actual solver executable
                                                    │
                                                    └── selected problem
```

**There is no Slurmy-generated `srun` in this chain.** `sbatch` creates the
allocation and starts `batch.sh`; the subsequent processes are launched directly
inside that allocation. A user-supplied command or recipe can contain its own
commands, but Slurmy does not insert an `srun` step.

## Source-file reference

| Component | Source |
| --- | --- |
| Workflow editor | `web-app/slurmy_web/templates/new.html`, `web-app/slurmy_web/static/builder.js` |
| HTTP routes and background operations | `web-app/slurmy_web/app.py` |
| Workflow validation and persistence | `web-app/slurmy_web/workflows.py` |
| Shared Makefile targets | `implementation/templates/workflow.mk` |
| All-pairs expansion | `implementation/slurmy-pairs.py` |
| Submission-plan generation | `implementation/slurmy.py` |
| Local submission script | `implementation/templates/submit.sh` |
| Remote build driver and generated `job.sh` | `implementation/build/slurmy-build.py` |
| Input packaging | `implementation/package_inputs.py` |
| Remote staging and publication | `implementation/templates/remote_prepare.sh`, `implementation/templates/remote_finish_upload.sh` |
| Remote allocation planning and submission loop | `implementation/templates/remote_submit.sh` |
| Actual solver-batch `sbatch` invocation | `implementation/submit_coordinator.py` |
| Compute-node batch execution | `implementation/templates/batch.sh` |
| Per-call execution and result publication | `implementation/templates/call.sh` |
| Timing wrapper | `implementation/templates/timed_call.sh` |
| SSH monitoring and result parsing | `web-app/slurmy_web/monitor.py` |

Files under `implementation/templates/` are source templates. Preparation copies
the relevant scripts into a workflow's `submit.sh.files/`; submission transfers
them into the remote job directory. The execution diagram describes those
generated and uploaded copies.
