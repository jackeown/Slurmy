"""Discovery modes keep shared fixtures and production imports available."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import os
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]


class TestLayoutTests(unittest.TestCase):
    def check_discovery(self, tree, direct):
        script = '''
import importlib
from pathlib import Path
import sys
import unittest
root = Path.cwd()
loader = unittest.TestLoader()
suite = loader.discover(sys.argv[1], pattern='test_*.py')
assert not loader.errors, '\\n'.join(loader.errors)
assert suite.countTestCases() > 0
if sys.argv[2] == 'implementation':
    import test_array_submission, test_batching, download
    assert test_array_submission.test_batching is test_batching
    assert Path(download.__file__).resolve() == root / 'web-app/download.py'
else:
    import test_workflow_audit_regressions, test_workflow_files
    assert test_workflow_audit_regressions.fixtures is test_workflow_files
'''
        start = f'{tree}/tests' if direct else tree
        env = dict(os.environ)
        if direct:
            env['PYTHONPATH'] = os.pathsep.join(str(ROOT / p) for p in ('implementation', 'web-app'))
        else:
            env.pop('PYTHONPATH', None)
        result = subprocess.run([sys.executable, '-c', script, start, tree],
                                cwd=ROOT, env=env, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_core_package_discovery(self):
        self.check_discovery('implementation', False)

    def test_web_package_discovery(self):
        self.check_discovery('web-app', False)

    def test_core_direct_discovery(self):
        self.check_discovery('implementation', True)

    def test_web_direct_discovery(self):
        self.check_discovery('web-app', True)
