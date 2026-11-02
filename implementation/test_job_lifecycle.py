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

    def test_transport_failure_retries_stop_and_delete_with_identical_payloads(self):
        scripts=[];results=iter([255,0,255,0])
        def run(*args,stdin,**kwargs):
            scripts.append((Path(stdin.name).name,stdin.read()))
            self.assertIn('BASH_ENV',args[0])
            return SimpleNamespace(returncode=next(results))
        with patch('sys.argv',['slurmy-job.py','delete','test-job']), patch.object(cli.subprocess,'run',side_effect=run), patch.object(cli.time,'sleep'):
            self.assertEqual(cli.main(),0)
        self.assertEqual([name for name,_ in scripts],['remote_stop.sh','remote_stop.sh','remote_delete.sh','remote_delete.sh'])
        self.assertEqual(scripts[0][1],scripts[1][1]);self.assertTrue(scripts[0][1])
        self.assertEqual(scripts[2][1],scripts[3][1])

    def test_stop_connection_failure_exhausts_retries_without_deleting(self):
        with patch('sys.argv',['slurmy-job.py','delete','test-job','--delete-local']), patch.object(cli.subprocess,'run',return_value=SimpleNamespace(returncode=255)) as run, patch.object(cli.time,'sleep'), patch.object(cli.shutil,'rmtree') as remove:
            self.assertEqual(cli.main(),255)
        self.assertEqual(run.call_count,3);remove.assert_not_called()

    def test_resume_is_not_blindly_replayed_after_ambiguous_network_failure(self):
        with patch('sys.argv',['slurmy-job.py','resume','test-job']), patch.object(cli.subprocess,'run',side_effect=[SimpleNamespace(returncode=0),SimpleNamespace(returncode=255)]) as run:
            self.assertEqual(cli.main(),255)
        self.assertEqual(run.call_count,2)

    def test_transport_timeout_is_retried_before_delete(self):
        with patch('sys.argv',['slurmy-job.py','delete','test-job']), patch.object(cli.subprocess,'run',side_effect=[cli.subprocess.TimeoutExpired('ssh',180),SimpleNamespace(returncode=0),SimpleNamespace(returncode=0)]) as run, patch.object(cli.time,'sleep'):
            self.assertEqual(cli.main(),0)
        self.assertEqual(run.call_count,3)

    def test_delete_waits_for_an_in_progress_request_and_bounds_retries(self):
        with patch('sys.argv',['slurmy-job.py','delete','test-job','--delete-local']), patch.object(cli.subprocess,'run',side_effect=[SimpleNamespace(returncode=0),SimpleNamespace(returncode=75),SimpleNamespace(returncode=75),SimpleNamespace(returncode=75)]) as run, patch.object(cli.time,'sleep'), patch.object(cli.shutil,'rmtree') as remove:
            self.assertEqual(cli.main(),75)
        self.assertEqual(run.call_count,4)
        remove.assert_not_called()
