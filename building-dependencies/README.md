# 🔨 Remote dependency builds

`slurmy-build.py` is the shared local driver for running a Bash build recipe in
a Slurm job and downloading its outputs. `runsolver/build.sh` is the bundled
runsolver source-build recipe. The three-file workflow automatically reuses
this driver for non-empty recipe lines in `building.txt`.

In the web workflow builder, configure each prover's recipe under **Provers**
and the limiter's recipe under **Limiter**. Choose an existing Bash script or
enter commands; a prebuilt executable can instead be packaged as-is. Dispatching
a workflow runs the declared recipes before packaging the job.

For standalone builds or custom build allocations:

```bash
python building-dependencies/slurmy-build.py \
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
Generated three-file workflows wrap recipes so both variables point to the
copied resource tree, then fetch that resulting tree back to the declared root.

The driver verifies the named artifacts before downloading with rsync.
Remote build directories and logs remain under `~/Slurmy-builds/` for inspection.
This tool does not execute the downloaded binaries on your computer.
