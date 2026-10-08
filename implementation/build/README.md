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
Recipes receive `SLURMY_BUILD_WORK`, the unpacked source directory, and
`SLURMY_BUILD_OUTPUT`, where standalone recipes must put downloadable outputs.
In the current JSON `building.txt` format, source files are unpacked into
`SLURMY_BUILD_WORK`; recipes put the declared executable under
`SLURMY_BUILD_OUTPUT`. Slurmy verifies that executable and downloads the output
directory into the runtime root. Legacy two-line build files still use the
older copied-root behavior.

The driver verifies the named artifacts before downloading with rsync.
Remote build directories and logs remain under `~/slurmy/builds/` for inspection.
This tool does not execute the downloaded binaries on your computer.
