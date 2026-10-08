# 📝 Workflow template

Copy this folder within `workflows/example-workflows`, or into
`workflows/my-workflows/NAME/`. Both locations use `REPO_ROOT := ../../..` in the Makefile;
the shared runsolver references keep the same relative paths.

1. Put your solver executable and supporting files in `bin/`, or change `solver_directory` and
   the resource root in the input files to existing directories elsewhere.
2. Edit `jobpairs.csv`: one command/problem per row, with all per-call limits
   and exclusivity choices.
3. Edit `building.txt`: the solver entry has an empty `recipe`, meaning no build.
   For remote compilation, provide a build-script path and its expected `artifact`
   relative to the runtime `root`. Set `source` to a local source directory, or
   leave it empty if the script fetches source itself. Add the script to
   `BUILD_RECIPES` in the Makefile so changes refresh submission files.
4. Review the runsolver invocation in `resource_limiter_template.txt`.
5. Set `BATCH_SIZE` (calls per Slurm task, run sequentially) in the Makefile,
   then `make prepare-submit`.

Paths in each specification resolve from the file that contains them. If you
put the workflow at a different depth, update `REPO_ROOT`, shared dependency
paths, and `BUILD_RECIPES` accordingly, or use absolute paths.

The placeholder `./my-solver` is deliberately not a supplied executable.
Runsolver is built remotely using the shared recipe.
Paths may point outside this folder; relative CSV paths resolve beside the CSV.

The [workflow guide](../../README.md) explains the shared Makefile targets.
Host and partition are configurable through `SLURMY_HOST` and
`SLURMY_PARTITION`. After changing input files or recipes, run `make` again
to refresh submission files. After changing `BATCH_SIZE`, run `make clean`,
then `make` when using a one-off command-line override. Editing the Makefile
itself refreshes scripts on the next `make`.

For all combinations, create a configurations table without the problem column
and use [slurmy-pairs.py](../../../implementation/slurmy-pairs.py). For guided setup, use the
[web workflow builder](../../../README.md): run `python web-app/main.py --new`
from the repository root. The web app can duplicate this example into `workflows/my-workflows`
and then edit its settings; replace the placeholder solver command before submitting.
