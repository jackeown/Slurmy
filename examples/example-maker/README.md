# Interactive Slurmy example maker

Launch the Textual workflow maker from this directory:

```bash
make
```

The four tabs collect the workflow identity, cluster, problem globs, remote
build recipe, solver invocation, per-call limits, Slurm requests, array layout,
and build resources. Review the summary and choose **Generate workflow**.
The build commands are trusted shell code that will run in a Slurm job, so
review them just as you would any build script.

The application creates a sibling `example-NAME/` directory using
`example-template/`. It writes no files until generation, never overwrites an
existing directory, and stays open so you can construct more than one workflow.
Afterward, add matching problems, inspect the generated files, and run `make`
inside the new directory.
