# 🧭 Your workflows

Open the local web app from the repository root:

```bash
python web-app/main.py --new
```

Saving creates `workflows/my-workflows/NAME/` directly. Each workflow contains `jobpairs.csv`,
`building.txt`, `resource_limiter_template.txt`, a Makefile, and saved form choices.
Configuration × problem workflows also retain their configurations and globs;
inline build commands become Bash scripts in the workflow folder.
Solver and problem resources stay at their existing paths until submission.
User workflow folders are ignored by Git.

Use the workflow page to edit, duplicate, prepare, and dispatch jobs. Its Makefile
also supports `make`, `make build`, `make prepare-submit` (refresh and dispatch),
`make submit` (dispatch already-prepared files), `make monitor`, `make sync`,
`make stop`, and `make clean`. A manually copied
[template](../example-workflows/example-template/README.md) uses `REPO_ROOT := ../../..`.

Deleted workflows are retained under `.deleted-workflows/` for recovery.

See the [main README](../../README.md) for setup and the web interface, the
[workflow guide](../README.md) for the files and Makefile, and the
[implementation reference](../../implementation/README.md) for input formats.
