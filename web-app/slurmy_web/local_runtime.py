"""Keep workflow subprocesses on the app's Python, including older Makefiles."""
from contextlib import contextmanager
import os
from pathlib import Path
import sys
import shlex
import tempfile


@contextmanager
def python_environment(environment):
    # Some existing/user workflows still invoke `python` by name. Do not depend
    # on a system alias or change the user's installation to accommodate them.
    with tempfile.TemporaryDirectory(prefix='slurmy-python-') as directory:
        for name in ('python', 'python3'):
            wrapper = Path(directory) / name
            # exec the original path: a symlink here can lose a venv's pyvenv.cfg.
            wrapper.write_text('#!/bin/sh\nexec ' + shlex.quote(sys.executable) + ' "$@"\n')
            wrapper.chmod(0o700)
        yield {**environment, 'SLURMY_PYTHON': sys.executable,
               'PATH': directory + os.pathsep + environment.get('PATH', '')}
