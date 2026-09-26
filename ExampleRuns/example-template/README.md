# 📝 Workflow template

Copy this folder within `ExampleRuns`, or into
`YourRuns/NAME/`. Both locations use `REPO_ROOT := ../..` in the Makefile;
the shared runsolver references keep the same relative paths.

1. Put your solver executable and supporting files in `bin/`, or change `solver_directory` and
   the resource root in the input files to existing directories elsewhere.
2. Edit `jobpairs.csv`: one command/problem per row, with all per-call limits
   and exclusivity choices.
3. Edit `building.txt`: the solver's blank recipe line currently means no build.
   Replace it with a build-script path if the executable needs remote compilation,
   and list that script in `BUILD_RECIPES` in the Makefile so changes refresh the
   packaged submission files.
4. Review the runsolver invocation in `resource_limiter_template.txt`.
5. Set `DEG_PAR` in the Makefile, then `make` and `make submit`.

Paths in each specification resolve from the file that contains them. If you
put the workflow at a different depth, update `REPO_ROOT`, shared dependency
paths, and `BUILD_RECIPES` accordingly, or use absolute paths.

The placeholder `./my-solver` is deliberately not a supplied executable.
Runsolver is built remotely using the shared recipe.
Paths may point outside this folder; relative CSV paths resolve beside the CSV.

Shared targets: `all` (prepare), `build`, `submit`, `monitor`, `sync`,
`stop`, and `clean`. Host and partition are configurable through
`SLURMY_HOST` and `SLURMY_PARTITION`. After changing input files or recipes,
run `make` again to refresh submission files. After changing `DEG_PAR`, run
`make clean`, then `make`.

For all combinations, create a configurations table without the problem column
and use [slurmy-pairs.py](../../slurmy-pairs.py). For guided setup, use the
[web workflow builder](../../README.md): run `python slurmy-web.py --new`
from the repository root. The web app can duplicate this example into `YourRuns`
and then edit its settings; replace the placeholder solver command before submitting.
