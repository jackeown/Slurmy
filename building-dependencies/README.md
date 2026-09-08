# Building dependencies on the cluster

This directory keeps the machinery and recipes for software that must be built
for the cluster's architecture:

```text
building-dependencies/
  slurmy-build.py       Submit any Bash build recipe as a Slurm job.
  slurmy-build.mk       Shared Make rules used by the example workflows.
  runsolver/
    build.sh            Remote runsolver recipe.
    runsolver           Downloaded cluster-built binary; ignored by Git.
```

`slurmy-build.py` stages a recipe on the SSH host, runs it on a compute node,
validates its declared artifacts, and downloads its output with `rsync`. See the
[main README](../README.md) for the command-line example and the environment
available to recipes.

The example Makefiles include `slurmy-build.mk`, so normal users can simply run
`make`; they do not need to call the builder directly.
