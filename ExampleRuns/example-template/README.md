# Slurmy workflow template

Copy this directory to `YourRuns/example-NAME`, then edit the workflow-variable
block in `Makefile`, `solver.solver`, and `build.sh`. Alternatively, launch the
guided generator in `YourRuns/example-generator/`; it creates configured
workflows under `YourRuns/GENERATED/`.

For an experiment using solver descriptions and problems that already exist
elsewhere, put their absolute locations in newline-delimited `solver-paths.txt`
and `problem-globs.txt` files, then set `SOLVER_PATHS_FILE` and
`PROBLEM_GLOBS_FILE`. The Makefile references them in place; `submit.sh`
packages them only when the experiment is submitted.

For the self-contained build-recipe mode, put benchmark files below
`problems/`. `build.sh` runs on a Slurm compute node and must install the
declared `SOLVER_ARTIFACT` below `$SLURMY_BUILD_OUTPUT`. Set `BUILD_CONTEXT` to
send an existing local source directory whose contents will be extracted below
`$SLURMY_BUILD_WORK`. In either mode, run:

```bash
make          # Build the solver and runsolver on the cluster, then generate submit.sh.
make submit   # Transfer the experiment and submit its Slurm array.
make monitor  # Inspect current and historical jobs interactively.
make sync     # Incrementally download the selected or latest job.
make stop     # Select an active Slurm job to cancel.
```

Use `SLURMY_HOST=another-cluster make` to override the default `datalab` SSH
host. Keep `EXCLUSIVE_NODES=no` to reserve only each task's requested cores or
physical CPU sockets and memory while leaving the rest of its node available. Set
`EXCLUSIVE_NODES=yes` only when every array task must reserve its assigned node
exclusively.

`CPU_REQUEST=4-core` reserves four cores. To reserve complete physical CPU
sockets instead, use `CPU_REQUEST=1-CPU` or `CPU_REQUEST=2-CPU` and set
`CORES_PER_CPU` to the physical core count of one socket (32 on `CPU-amd` at
the time this template was written). Slurmy then requests and packs every core
of those sockets. `make build`, `make clean`, and `make distclean` are also
available.
