"""Large axiom sets prepare atomically without per-file shell subprocesses."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
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
    def test_full_cluster_collection_glob_uses_selected_root_without_copying(self):
        import shutil
        from slurmy import generate
        import test_batching
        for version in ('TPTP-v10.0.0', 'TPTP-v9.2.1'):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as folder:
                root=Path(folder);assets=test_batching.SequentialBatchTests().prepare(root,[(False,False)],1)
                shutil.rmtree(assets);(root/'submit.sh').unlink()
                axioms=root/'axiom-globs.txt';axioms.write_text(f'tptp://{version}/Axioms/**/*.ax\n')
                with patch('slurmy.read_axioms') as expand, patch.dict(os.environ,SLURMY_TPTP_ROOT='/other'):
                    generate(root/'jobpairs.csv',root/'building.txt',root/'limiter.txt',axioms)
                expand.assert_not_called()
                config=(assets/'calls/0/config.sh').read_text()
                self.assertIn(f'TPTP_DEFAULT_ROOT=/share/datasets/TPTP/{version}\n',config)
                self.assertIn("TPTP_ROOT_REL=''",config)
                self.assertFalse((assets/'prepare_axioms.sh').exists())
                self.assertFalse((assets/'axiom-paths.json').exists())

    def test_subset_or_mixed_axiom_globs_keep_job_local_preparation(self):
        from slurmy import shared_axiom_root
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'axiom-globs.txt'
            for patterns in ('tptp://TPTP-v10.0.0/Axioms/SET*.ax\n',
                             'tptp://TPTP-v10.0.0/Axioms/**/*.ax\n/local/extra.ax\n'):
                source.write_text(patterns)
                self.assertIsNone(shared_axiom_root(source))

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

    def test_shared_axioms_are_read_directly_and_not_packaged_as_local_files(self):
        import shutil
        from slurmy import generate
        import test_batching
        source=Path('tptp:/share/datasets/TPTP/TPTP-v9.2.1/Axioms/SYN001+0.ax')
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);assets=test_batching.SequentialBatchTests().prepare(root,[(False,False)],1)
            shutil.rmtree(assets);(root/'submit.sh').unlink()
            with patch('slurmy.read_axioms',return_value=([source],{source:Path('SYN001+0.ax')})):
                generate(root/'jobpairs.csv',root/'building.txt',root/'limiter.txt')
            self.assertNotIn('tptp:',(assets/'archive-paths.txt').read_text())
            self.assertEqual(json.loads((assets/'axiom-paths.json').read_text()),[[str(source),'SYN001+0.ax']])
            def copy(source,target):
                self.assertEqual(str(source),'/share/datasets/TPTP/TPTP-v9.2.1/Axioms/SYN001+0.ax')
                target.write_text('shared axiom')
            with patch('prepare_axioms.shutil.copyfile',side_effect=copy):prepare(assets)
            self.assertEqual((assets/'rootfs/.slurmy-tptp/Axioms/SYN001+0.ax').read_text(),'shared axiom')

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
                result = subprocess.run([sys.executable, str(Path(__file__).parent.parent / 'prepare_axioms.py')],
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
            result = subprocess.run([sys.executable, str(Path(__file__).parent.parent / 'prepare_axioms.py')],
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
