"""Destructive lifecycle actions must finish stopping before proceeding."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('job_cli', Path(__file__).with_name('slurmy-job.py'))
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class LifecycleTests(unittest.TestCase):
    def test_delete_and_resume_stop_first(self):
        for action in ('delete', 'resume'):
            scripts = []
            def run(*args, stdin, **kwargs):
                scripts.append(Path(stdin.name).name)
                return SimpleNamespace(returncode=0)
            with self.subTest(action=action), patch('sys.argv', ['slurmy-job.py', action, 'test-job']), patch.object(cli.subprocess, 'run', side_effect=run):
                self.assertEqual(cli.main(), 0)
            self.assertEqual(scripts, ['remote_stop.sh', f'remote_{action}.sh'])

    def test_failed_stop_preserves_job_and_local_results(self):
        with patch('sys.argv', ['slurmy-job.py', 'delete', 'test-job', '--delete-local']), patch.object(cli.subprocess, 'run', return_value=SimpleNamespace(returncode=1)) as run, patch.object(cli.shutil, 'rmtree') as remove:
            self.assertEqual(cli.main(), 1)
        self.assertEqual(run.call_count, 1)
        remove.assert_not_called()

    def test_failed_cluster_deletion_keeps_local_results(self):
        with patch('sys.argv', ['slurmy-job.py', 'delete', 'test-job', '--delete-local']), patch.object(cli.subprocess, 'run', side_effect=[SimpleNamespace(returncode=0), SimpleNamespace(returncode=1)]), patch.object(cli.shutil, 'rmtree') as remove:
            self.assertEqual(cli.main(), 1)
        remove.assert_not_called()

    def test_automatic_resume_rechecks_idle_state_without_cancelling_tasks(self):
        scripts = []
        def run(*args, stdin, **kwargs):
            scripts.append(stdin.read().decode())
            return SimpleNamespace(returncode=0)
        with patch('sys.argv', ['slurmy-job.py', 'resume', 'test-job', '--if-idle']), patch.object(cli.subprocess, 'run', side_effect=run):
            self.assertEqual(cli.main(), 0)
        self.assertEqual(len(scripts), 1)
        self.assertIn('SLURM tasks are active; no recovery needed.', scripts[0])
        self.assertIn('keeping it stopped', scripts[0])
        self.assertNotIn('scancel', scripts[0])
