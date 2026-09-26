# 🧭 Your workflows

Open the local web app from the repository root:

```bash
python slurmy-web.py --new
```

Saving creates `YourRuns/NAME/` directly. Each workflow contains `jobpairs.csv`,
`building.txt`, `resource_limiter_template.txt`, a Makefile, and saved form choices.
Configuration × problem workflows also retain their configurations and globs;
inline build commands become Bash scripts in the workflow folder.
Solver and problem resources stay at their existing paths until submission.
User workflow folders are ignored by Git.

Use the workflow page to edit, duplicate, prepare, and dispatch jobs. Its Makefile
also supports `make`, `make build`, `make submit`, `make monitor`, `make sync`,
`make stop`, and `make clean`. A manually copied
[template](../ExampleRuns/example-template/README.md) uses `REPO_ROOT := ../..`.

The updated app automatically moves older workflows out of the former
`GENERATED` subfolder and preserves links to their past jobs. Deleted workflows
are retained under `.deleted-workflows/` for recovery.

See the [main README](../README.md) for setup, the web interface, and the
complete specification format.
