# 🧪 Example workflows

Each workflow uses the same three-file interface documented in the
[main README](../../README.md). Builds happen on Slurm compute nodes, not your laptop.

| Folder | Workflow |
| --- | --- |
| [example-vampire](example-vampire/) | Vampire, built from the included source snapshot. |
| [example-e](example-e/) | E, built from source. |
| [example-drodi](example-drodi/) | Drodi, built from source. |
| [example-combined](example-combined/) | All three configurations × ten shared problems. |
| [example-template](example-template/README.md) | Minimal starting point for your own solver. |

## 🚀 Run an example

After completing the main README's setup, run `python web-app/main.py` from the
repository root. Open **Workflows → Example workflows**, choose an example, and
use **Prepare & dispatch job**, or open its arrow menu for dispatch-only or
prepare-only. To change the
settings, duplicate the example into `workflows/my-workflows` and select **Edit settings**.
Solver commands and builds are together under Solvers; the limiter's invocation
and build are together under Limiter.

The same examples work from the command line:

```bash
cd workflows/example-workflows/example-combined
make                          # Generate jobpairs.csv and inspectable submit scripts.
make prepare-submit           # Refresh scripts, build dependencies, then submit.
make submit                   # Dispatch the already-prepared scripts without refreshing them.
make monitor                  # Start/reuse the local web app and open this workflow.
make sync                     # Continuously download the latest job's results.
make stop                     # Select an active job to cancel.
```

Use `SLURMY_HOST=your-alias SLURMY_PARTITION=your-partition make prepare-submit`
to override the example defaults (`datalab`, `CPU-amd`).
To change batch size for one run, use `make clean`, then
`make BATCH_SIZE=10` (or edit the Makefile). Calls within a batch run sequentially.
Command-line Make variables alone do not invalidate existing submission scripts;
editing the Makefile does. After changing inputs or recipes, run `make` to
refresh the scripts automatically.

## 📁 What to edit

- `configurations.csv`: solver commands, limits, and core/socket/node choices.
- `building.txt`: runtime roots, optional source directories, build recipes, and expected executables, including runsolver.
- `resource_limiter_template.txt`: limiter invocation.
- `Makefile`: problem globs, batch size, and optional
  `PAIR_ORDER` (`problem-major`, `solver-major`, or `random`).

The solver examples use 3-second CPU limits and 6-second wall limits.
The combined example selects five easy and five hard problems under
[problems](../example-problems/combined/). Its resources reference the individual solver folders.
The union of all selected globs is crossed with every configuration.

Each example points its build's `source` field at the corresponding checked-in
directory under [example-solvers](../example-solvers/). Slurmy uploads that source
to the cluster build job's `$SLURMY_BUILD_WORK`; the recipe compiles it there
and writes the executable to `$SLURMY_BUILD_OUTPUT`. Slurmy downloads the output
into the example's `bin/` runtime root, which is then packaged for submission.
This is the same pattern to use with your own local solver source.

`make` does not compile or submit. `make build` explicitly builds and downloads
resources; `make submit` builds them again before packaging.
`make clean` removes only `submit.sh` and `submit.sh.files/`, retaining inputs
and downloaded binaries. In the four solver examples, `make pairs-clean`
also removes the generated `jobpairs.csv`, so edits to configurations or problem
selection can be expanded again.
