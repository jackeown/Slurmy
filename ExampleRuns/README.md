# 🧪 Example workflows

Each workflow uses the same three-file interface documented in the
[main README](../README.md). Builds happen on Slurm compute nodes, not your laptop.

| Folder | Workflow |
| --- | --- |
| [example-vampire](example-vampire/) | Latest Vampire, built from source. |
| [example-e](example-e/) | E, built from source. |
| [example-drodi](example-drodi/) | Drodi, built from source. |
| [example-combined](example-combined/) | All three configurations × ten shared problems. |
| [example-template](example-template/README.md) | Minimal starting point for your own solver. |

## 🚀 Run an example

After completing the main README's setup, run `python slurmy-web.py` from the
repository root. Open **Workflows → Example workflows**, choose an example, and
use **Prepare Slurm submission** or **Dispatch new Slurm job**. To change the
settings, duplicate the example into `YourRuns` and select **Edit settings**.
Prover commands and builds are together under Provers; the limiter's invocation
and build are together under Limiter.

The same examples work from the command line:

```bash
cd ExampleRuns/example-combined
make                          # Generate jobpairs.csv and inspectable submit scripts.
make submit                   # Build all dependencies remotely, then submit.
make monitor                  # Start/reuse the local web app and open this workflow.
make sync                     # Continuously download the latest job's results.
make stop                     # Select an active job to cancel.
```

Use `SLURMY_HOST=your-alias SLURMY_PARTITION=your-partition make submit`
to override the example defaults (`datalab`, `CPU-amd`).
To change within-batch concurrency, run `make clean`, then `make DEG_PAR=2`
(or edit `DEG_PAR` in the Makefile). Changing only a command-line Make variable
does not invalidate existing submission scripts. After changing input files or
recipes, run `make`; Slurmy refreshes the scripts automatically.

## 📁 What to edit

- `configurations.csv`: solver commands, limits, and core/socket/node choices.
- `building.txt`: resource roots and build recipes, including runsolver.
- `resource_limiter_template.txt`: limiter invocation.
- `Makefile`: problem globs and degree of parallelism.

The prover examples use 3-second CPU limits and 6-second wall limits.
The combined example selects five easy and five hard problems under
[problems](example-combined/problems/). Its resources reference the individual prover folders.
The union of all selected globs is crossed with every configuration.

Each prover's cluster-built executable is downloaded into its `bin/` directory.
That directory is packaged for submission and used as the solver's working
directory. Source checkout and compilation happen in temporary directories on
the cluster; no separate local `resource/` directory is needed.

`make` does not compile or submit. `make build` explicitly builds and downloads
resources; `make submit` builds them again before packaging.
`make clean` removes only `submit.sh` and `submit.sh.files/`, retaining inputs
and downloaded binaries. In the four prover examples, `make pairs-clean`
also removes the generated `jobpairs.csv`, so edits to configurations or problem
selection can be expanded again.
