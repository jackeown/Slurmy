# Interactive Slurmy example generator

Launch the Textual workflow generator from this directory:

```bash
make
```

The generator asks one question at a time and supplies no default answers. After
collecting the workflow identity and cluster, it asks whether you already have
a solver-description file or want to create one interactively.

- **Existing description:** enter one path or glob per line. Every matched
  description must be readable, name an existing solver root on its first
  line, and contain at least one invocation.
- **Create a description:** enter an existing solver root, followed by one or
  more invocation lines. Every invocation must contain `{{problem}}` or
  `{{problem=path}}`.

Problem globs are also validated immediately, and every glob must match at
least one existing regular file. Relative solver and problem paths always
start from this `examples/example-generator/` directory—even if the application
is launched elsewhere. The generator converts them to absolute paths before
showing the review screen or generating files.

The application creates `examples/GENERATED/example-NAME/` using
`example-template/`. It writes no files until generation and never overwrites
an existing directory. Keeping generated workflows under `GENERATED/` makes
them easy to distinguish from the maintained examples. Solver roots and
problems can live anywhere on the local filesystem: the generated workflow
records their absolute locations but does not copy them. `workflow.mk` points
to `solver-paths.txt` and
`problem-globs.txt`, which contain those absolute locations. When you create a
description interactively, the generated `solver.solver` also contains the
absolute solver-root path. The usual submission packaging transfers the
required solver and problem files later.

Afterward, inspect `workflow.mk` and run `make` inside the new directory:

```bash
cd ../GENERATED/example-NAME
make
```
