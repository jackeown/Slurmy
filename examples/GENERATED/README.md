# Generated workflows

The interactive [`example-generator`](../example-generator/) places every
generated workflow in this directory as `example-NAME/`.

These workflow directories contain machine-specific absolute paths and are
ignored by Git. After generating one, review its `workflow.mk`, then run:

```bash
cd example-NAME
make
make submit
make monitor
```
