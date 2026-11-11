"""Large axiom sets prepare atomically without per-file shell subprocesses."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from prepare_axioms import prepare


class AxiomPreparationTests(unittest.TestCase):
    def test_generated_workflow_uses_one_python_preparation_process(self):
        import shutil
        from slurmy import generate
        import test_batching
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            assets = test_batching.SequentialBatchTests().prepare(root, [(False, False)], 1)
            shutil.rmtree(assets); (root/'submit.sh').unlink()
            axiom = root/'a+1^2.ax'; axiom.write_text('axiom')
            with patch('slurmy.read_axioms', return_value=([axiom], {axiom: Path('nested/a+1^2.ax')})):
                generate(root/'jobpairs.csv', root/'building.txt', root/'limiter.txt')
            self.assertTrue((assets/'prepare_axioms.py').is_file())
            self.assertEqual(json.loads((assets/'axiom-paths.json').read_text()), [[str(axiom), 'nested/a+1^2.ax']])
            self.assertIn('exec python3', (assets/'prepare_axioms.sh').read_text())
            self.assertNotIn('cp --', (assets/'prepare_axioms.sh').read_text())

    def test_large_set_exact_names_nested_paths_and_retry(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            inputs = job / 'rootfs/source'; inputs.mkdir(parents=True)
            paths = []
            for index in range(1001):
                name = f"axiom_{index}+x^y's.ax"
                (inputs / name).write_text(str(index))
                paths.append(('/source/' + name, 'nested/' + name))
            (job / 'axiom-paths.json').write_text(json.dumps(paths))
            for _ in range(2):
                result = subprocess.run([sys.executable, str(Path(__file__).parent / 'prepare_axioms.py')],
                                        env={**os.environ, 'JOB_DIR': str(job)}, capture_output=True, text=True, check=True)
                self.assertIn('1001/1001', result.stdout)
            target = job / 'rootfs/.slurmy-tptp/Axioms/nested'
            self.assertEqual(len(list(target.iterdir())), 1001)
            self.assertEqual((target / paths[-1][1].split('/')[-1]).read_text(), '1000')

    def test_missing_source_retains_previous_complete_file_and_reports_path(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            target = job / 'rootfs/.slurmy-tptp/Axioms/a.ax'
            target.parent.mkdir(parents=True); target.write_text('previous complete contents')
            (job / 'axiom-paths.json').write_text(json.dumps([('/missing/a.ax', 'a.ax')]))
            result = subprocess.run([sys.executable, str(Path(__file__).parent / 'prepare_axioms.py')],
                                    env={**os.environ, 'JOB_DIR': str(job)}, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('missing/a.ax', result.stderr)
            self.assertEqual(target.read_text(), 'previous complete contents')
            self.assertFalse(list(target.parent.glob('*.slurmy-copy-tmp')))

    def test_rejects_parent_traversal(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            (job / 'axiom-paths.json').write_text(json.dumps([('/source/a.ax', '../escape.ax')]))
            with self.assertRaisesRegex(ValueError, 'Invalid packaged'):
                prepare(job)
