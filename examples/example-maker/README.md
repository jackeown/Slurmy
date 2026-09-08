# Interactive Slurmy example maker

Launch the Textual workflow maker from this directory:

```bash
make
```

The maker asks one question at a time and supplies no default answers. It
collects the workflow identity, cluster, existing solver-description paths,
existing problem globs, per-call limits, Slurm requests, and array layout.

Solver and problem paths are expanded and validated immediately before the
maker advances. Each solver description must be readable, name an existing
solver root on its first line, and contain at least one invocation. Every
problem glob must match at least one existing regular file.

The application creates a sibling `example-NAME/` directory using
`example-template/`. It writes no files until generation and never overwrites
an existing directory. Solver roots and problems can live anywhere on the
local filesystem: the generated workflow records their absolute locations but
does not copy them. The usual submission packaging transfers them later.

Afterward, inspect `workflow.mk` and run `make` inside the new directory.
