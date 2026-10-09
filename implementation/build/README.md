# 🔨 Remote dependency builds

`slurmy-build.py` is the shared local driver for running a Bash build recipe in
a Slurm job and downloading its outputs. `runsolver/build.sh` is the included
runsolver source-build recipe. The three-file workflow automatically reuses
this driver for each resource with a recipe in `building.txt`.

In the web workflow builder, configure each solver's recipe under **Solvers**
and the limiter's recipe under **Limiter**. Choose an existing Bash script or
enter commands; a prebuilt executable can instead be packaged as-is. Dispatching
a workflow runs the declared recipes before packaging the job.

For standalone builds or custom build allocations:

```bash
python implementation/build/slurmy-build.py \
    --name my-solver \
    --context /path/to/sources \
    --recipe /path/to/build.sh \
    --output /path/to/downloaded-artifacts \
    --artifact bin/my-solver \
    --cpus-per-task 4 --memory 4GiB --time 00:30:00 \
    --sbatch-option=--partition=CPU-amd
```

`--host` overrides `SLURMY_HOST` (default `datalab`).
Standalone builds use `SLURMY_BUILD_WORK` (the unpacked source) and
`SLURMY_BUILD_OUTPUT` (downloaded artifacts) by default. Pass `--in-place`
when a conventional build script instead writes into its current directory;
the whole resulting directory is downloaded. Workflows select in-place mode
automatically when their source and runtime root are the same folder. Recipes
for a separate source and runtime root may still write selected files to
`SLURMY_BUILD_OUTPUT`. Slurmy verifies the declared artifact in either mode.

The driver verifies the named artifacts before downloading with rsync.
Remote build directories and logs remain under `~/slurmy/builds/` for inspection.
This tool does not execute the downloaded binaries on your computer.
