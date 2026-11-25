"""Python aliases need not be installed or match the app's interpreter."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
from pathlib import Path
import subprocess
import os
import sys
import unittest
import tempfile
import venv
from unittest.mock import patch

from slurmy_web.local_runtime import python_environment, workflow_environment


class LocalRuntimeTests(unittest.TestCase):
    def test_empty_path_reuses_current_interpreter_and_cleans_up(self):
        original = {'PATH': '', 'SLURMY_PYTHON': '/wrong/python', 'KEEP': 'value'}
        with python_environment(original) as env:
            directory = Path(env['PATH'].split(':')[0])
            self.assertEqual(env['SLURMY_PYTHON'], sys.executable)
            self.assertEqual(env['KEEP'], 'value')
            for name in ('python', 'python3'):
                result = subprocess.run([name, '-c', 'import sys; print(sys.prefix)'],
                                        env=env, text=True, capture_output=True, check=True)
                self.assertEqual(result.stdout.strip(), sys.prefix)
        self.assertFalse(directory.exists())
        self.assertEqual(original['PATH'], '')

    def test_fresh_workflow_make_ignores_inherited_reservation_overrides(self):
        inherited={**os.environ, 'MAKEFLAGS':'-- SLURMY_RESERVATION=slurm-cpu',
                   'MAKEOVERRIDES':'SLURMY_RESERVATION=slurm-cpu', 'MFLAGS':'-e',
                   'GNUMAKEFLAGS':'-e', 'MAKELEVEL':'3', 'SLURMY_RESUME_JOB_ID':'old-job',
                   'SLURMY_RESERVATION':'old-shell-value', 'KEEP':'yes'}
        clean=workflow_environment(inherited)
        self.assertEqual(clean['KEEP'],'yes')
        for name in ('MAKEFLAGS','MAKEOVERRIDES','MFLAGS','GNUMAKEFLAGS','MAKELEVEL','SLURMY_RESUME_JOB_ID'):
            self.assertNotIn(name,clean)
            self.assertIn(name,inherited)
        with tempfile.TemporaryDirectory() as folder:
            makefile=Path(folder)/'Makefile'
            for value in ('','new-reservation'):
                makefile.write_text('SLURMY_RESERVATION := '+value+'\nexport SLURMY_RESERVATION\nshow:\n\t@printf "<%s>" "$$SLURMY_RESERVATION"\n')
                result=subprocess.run(['make','--no-print-directory','-f',str(makefile),'show'],env=clean,capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(result.stdout,'<'+value+'>')

    def test_exception_cleans_up(self):
        with self.assertRaisesRegex(RuntimeError, 'failed'):
            with python_environment({}) as env:
                directory = Path(env['PATH'].split(':')[0])
                raise RuntimeError('failed')
        self.assertFalse(directory.exists())

    def test_virtual_environment_is_preserved_even_with_spaces_in_path(self):
        with tempfile.TemporaryDirectory(prefix='slurmy venv ') as folder:
            venv.EnvBuilder(with_pip=False).create(folder)
            interpreter = str(Path(folder) / 'bin/python')
            with patch('slurmy_web.local_runtime.sys.executable', interpreter):
                with python_environment({'PATH': ''}) as env:
                    result = subprocess.run(['python', '-c', 'import sys; print(sys.prefix)'],
                                            env=env, capture_output=True, text=True, check=True)
                    self.assertEqual(Path(result.stdout.strip()), Path(folder))
