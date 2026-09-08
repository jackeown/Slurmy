# Slurmy workflow template

Copy this directory to `examples/example-NAME`, then edit the workflow-variable
block in `Makefile`, `solver.solver`, and `build.sh`. Alternatively, launch the
guided generator in `../example-generator/`; it creates configured workflows
under `../GENERATED/`.

For an experiment using solver descriptions and problems that already exist
elsewhere, put their absolute locations in newline-delimited `solver-paths.txt`
and `problem-globs.txt` files, then set `SOLVER_PATHS_FILE` and
`PROBLEM_GLOBS_FILE`. The Makefile references them in place; `submit.sh`
packages them only when the experiment is submitted.

For the self-contained build-recipe mode, put benchmark files below
`problems/`. In either mode, run:

```bash
make          # Build the solver and runsolver on the cluster, then generate submit.sh.
make submit   # Transfer the experiment and submit its Slurm array.
make monitor  # Inspect current and historical jobs interactively.
make sync     # Incrementally download the selected or latest job.
```

Use `SLURMY_HOST=another-cluster make` to override the default `datalab` SSH
host. `make build`, `make clean`, and `make distclean` are also available.
