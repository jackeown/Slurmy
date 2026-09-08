# Slurmy workflow template

Copy this directory to `examples/example-NAME`, then edit the workflow-variable
block in `Makefile`, `solver.solver`, and `build.sh`. Alternatively, launch the
guided maker in `../example-maker/` and let it create those files.

Put benchmark files below `problems/`, then run:

```bash
make          # Build the solver and runsolver on the cluster, then generate submit.sh.
make submit   # Transfer the experiment and submit its Slurm array.
make monitor  # Inspect current and historical jobs interactively.
make sync     # Incrementally download the selected or latest job.
```

Use `SLURMY_HOST=another-cluster make` to override the default `datalab` SSH
host. `make build`, `make clean`, and `make distclean` are also available.
