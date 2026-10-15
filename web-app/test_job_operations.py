"""Regression checks for job action coordination, without contacting Slurm."""
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from threading import Barrier, Thread
from unittest.mock import Mock, patch

from slurmy_web import app as web


class JobOperationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        for target, value in (
                ('STATE', Path(temporary.name)),
                ('workflows.migrate_user_runs', Mock()),
                ('threading.Thread', Mock())):
            patcher = patch.object(web, target, value) if '.' not in target else patch(
                'slurmy_web.app.' + target, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        self.job = SimpleNamespace(percent=20, submission_state='submitting', active_records=[], task_count=10)
        self.fetch = Mock(side_effect=lambda *args: SimpleNamespace(find_job=lambda job_id, job=self.job: job))
        app = web.create_app()
        self.app = app
        app.config['COLLECTOR'] = lambda *args: SimpleNamespace(fetch=self.fetch)
        self.client = app.test_client()
        with self.client.session_transaction() as session:
            session['csrf'] = 'test-token'
        self.headers = {'X-CSRF-Token': 'test-token'}

    def action(self, action):
        return self.client.post('/api/jobs/test-job/action', json={'action': action}, headers=self.headers)

    def test_repeated_stop_reuses_running_operation(self):
        first, second = self.action('stop'), self.action('stop')
        self.assertEqual(first.status_code, 202)
        self.assertEqual(second.status_code, 202)
        self.assertEqual(first.json['id'], second.json['id'])
        self.assertEqual(web.threading.Thread.call_count, 1)

    def test_download_does_not_block_stop(self):
        download = self.action('sync')
        stop = self.action('stop')
        self.assertEqual(download.status_code, 202)
        self.assertEqual(stop.status_code, 202)
        self.assertNotEqual(download.json['id'], stop.json['id'])
        self.assertEqual(self.action('resume').status_code, 409)

    def test_simultaneous_stop_requests_start_only_one_worker(self):
        barrier = Barrier(2)
        responses = []

        def request_stop():
            with self.app.test_client() as client:
                with client.session_transaction() as session:
                    session['csrf'] = 'test-token'
                barrier.wait(timeout=5)
                response = client.post('/api/jobs/test-job/action', json={'action': 'stop'},
                                       headers=self.headers)
                responses.append((response.status_code, response.json))

        threads = [Thread(target=request_stop) for _ in range(2)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=10)
            self.assertFalse(thread.is_alive())
        self.assertEqual(len(responses), 2)
        self.assertEqual([status for status, _ in responses], [202, 202])
        self.assertEqual(responses[0][1]['id'], responses[1][1]['id'])
        self.assertEqual(web.threading.Thread.call_count, 1)

    def test_resume_does_not_block_stop(self):
        self.assertEqual(self.action('resume').status_code, 202)
        self.assertEqual(self.action('stop').status_code, 202)

    def test_delete_refreshes_cached_state_and_blocks_active_operation(self):
        self.action('sync')  # Populate a cached, unfinished snapshot.
        self.job = SimpleNamespace(percent=20, submission_state='stopped', active_records=[], task_count=10)
        # A fresh stopped snapshot must still not allow deleting an active download.
        response = self.client.post('/api/jobs/test-job/delete', json={}, headers=self.headers)
        self.assertEqual(self.fetch.call_count, 2)
        self.assertEqual(response.status_code, 409)
        self.assertIn('active job operation', response.json['error'])

    def test_delete_rechecks_slurm_after_stop(self):
        self.action('unknown')  # Cache the unfinished job without starting work.
        self.job = SimpleNamespace(percent=20, submission_state='stopped', active_records=[], task_count=10)
        with patch.object(web.subprocess, 'run', return_value=SimpleNamespace(
                returncode=1, stderr='Slurm still has an active allocation for this job.')) as run:
            response = self.client.post('/api/jobs/test-job/delete', json={}, headers=self.headers)
        self.assertEqual(self.fetch.call_count, 2)
        self.assertEqual(response.status_code, 409)
        self.assertIn('active allocation', response.json['error'])
        script = run.call_args.kwargs['input']
        self.assertLess(script.index('flock -n 9'), script.index('squeue'))
        self.assertLess(script.index('squeue'), script.index('rm -r'))

    def test_stopped_idle_job_can_be_deleted(self):
        self.job = SimpleNamespace(percent=20, submission_state='stopped', active_records=[], task_count=10)
        with patch.object(web.subprocess, 'run', return_value=SimpleNamespace(returncode=0)):
            response = self.client.post('/api/jobs/test-job/delete', json={}, headers=self.headers)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json['deleted'], 'test-job')

    def test_delete_unfinished_job_is_rejected(self):
        with patch.object(web.subprocess, 'run') as run:
            response = self.client.post('/api/jobs/test-job/delete', json={}, headers=self.headers)
        self.assertEqual(response.status_code, 409)
        run.assert_not_called()


if __name__ == '__main__':
    unittest.main()
