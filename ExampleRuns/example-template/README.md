# 📝 Experiment template

Copy this folder within ExampleRuns, or into YourRuns/GENERATED (then change
`REPO_ROOT` in the Makefile to `../../..`).

1. Put your solver executable and supporting files in `bin/`, or change `solver_directory` and
   the resource root in the input files to existing directories elsewhere.
2. Edit `jobpairs.csv`: one command/problem per row, with all per-call limits
   and exclusivity choices.
3. Edit `building.txt`: the solver's blank recipe line currently means no build.
   Replace it with a build-script path if the executable needs remote compilation.
4. Review the runsolver invocation in `resource_limiter_template.txt`.
5. Set `DEG_PAR` in the Makefile, then `make` and `make submit`.

The placeholder `./my-solver` is deliberately not a supplied executable.
Runsolver is built remotely using the shared recipe.
Paths may point outside this folder; relative CSV paths resolve beside the CSV.

Shared targets: `all` (prepare), `build`, `submit`, `monitor`, `sync`,
`stop`, and `clean`. Host and partition are configurable through
`SLURMY_HOST` and `SLURMY_PARTITION`. After changing inputs or degree, run
Running `make` again refreshes generated submission files when these inputs
change.

For all combinations, create a configurations table without the problem column
and use [slurmy-pairs.py](../../slurmy-pairs.py). For guided setup, use the
[web experiment builder](../../YourRuns/example-generator/README.md).
