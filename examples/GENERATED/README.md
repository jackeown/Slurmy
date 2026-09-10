# Generated workflows

The interactive [`example-generator`](../example-generator/) places every
generated workflow in this directory as `example-NAME/`.

These workflow directories contain machine-specific absolute paths and are
ignored by Git. Depending on the generator choices, `make` either uses an
existing solver or first builds it in a Slurm job and downloads the resulting
executable. After generating one, review its `workflow.mk`, then run:

```bash
cd example-NAME
make
make submit
make monitor
```
