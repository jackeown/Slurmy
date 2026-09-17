# 🧭 Web experiment builder

```bash
make -C YourRuns/example-generator
# or: make -C YourRuns/example-generator monitor
```

Both commands start the shared local Flask app if needed and open its builder
page in your browser. There is no separate terminal dashboard.

The five steps cover the workflow name and cluster settings, solver/problem
calls, optional remote builds, the resource limiter, and a validated preview.
Settings start blank: choose them explicitly.

Use an existing jobpairs CSV, import a configurations CSV, or enter configurations
in the form and select problem globs. The union of the globs is crossed with each
configuration. Include all per-call limits, core/socket counts, and both
exclusivity choices. Resource roots can be packaged as-is, built by an existing
Bash script, or built using commands entered in the form.

Path fields are checked when you leave them; a full preview validates the
specification before saving. Relative paths you enter resolve from
`YourRuns/example-generator/`. Paths inside imported files resolve from those
files. Absolute paths are saved, and source/problem files stay where they are.

Saving creates `YourRuns/GENERATED/example-NAME/` with the three input files,
a Makefile, and any newly entered build recipes. It does not build or submit.
From the resulting experiment page, inspect the inputs, prepare scripts, and
explicitly confirm building/submission. Progress appears in the operation log.

The same workflow still supports `make`, `make submit`, `make monitor`,
`make sync`, `make stop`, and `make clean`. All-pairs workflows retain
their configurations and globs; move the old jobpairs.csv aside before using
`make pairs`.

See the [main README](../../README.md) for installation, server lifecycle,
localhost-only security, and command-line alternatives.
