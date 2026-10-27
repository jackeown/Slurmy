# 🔨 Remote dependency builds

`slurmy-build.py` is the shared local driver for running a Bash build recipe in
a SLURM job and downloading its outputs. `runsolver/build.sh` is the included
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

## Reusing successful builds

The workflow editor offers **Build on every run?** for solver and resource builds:
**build for every job** (the existing default), or **only build on first job or
when source changes**. Standalone builds accept `--build-policy changed`.

Slurmy hashes the source tree contents, paths and permissions, recipe, build
settings and driver. It also verifies that downloaded outputs are intact.
A successful build/download records a local receipt in the runtime root;
unchanged inputs and outputs reuse that build without submitting a build job.
In-place builds record the tree after downloading, so generated build files do
not trigger a rebuild on every job. Failed builds invalidate the receipt. If
local inputs change during a build, its result is not cached. Builds sharing a
runtime root are serialized.

This tracks local files, not remote Git branches, downloaded dependencies,
cluster modules, or other external state. Choose **build for every job** to
refresh those. Source/output trees containing symlinks or special files build
normally but do not reuse builds, because their external inputs cannot safely
be inferred. Workflow exports preserve the selected policy.
