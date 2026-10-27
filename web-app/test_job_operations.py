"""Regression checks for job action coordination, without contacting SLURM."""
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

    def test_successful_resume_is_not_reclassified_as_a_new_submission(self):
        def resumed(*args, stdout, **kwargs):
            stdout.write('Keeping previously completed results.\nSlurmy job ID: test-job\nSubmission complete: 2/2 batches accepted\n')
            return SimpleNamespace(returncode=0)
        with patch.dict(web.os.environ, {}, clear=True), patch.object(web.subprocess, 'run', side_effect=resumed):
            response = self.action('resume')
            self.assertEqual(response.status_code, 202)
            worker = web.threading.Thread.call_args.kwargs['target']
            worker()
        state = self.client.get('/api/operations/'+response.json['id']).json
        self.assertEqual(state['state'], 'done')
        self.assertEqual(state['code'], 0)
        self.assertEqual(state['phase'], 'Resume unfinished solver-problem pairs')
        self.assertIn('Keeping previously completed results.', state['log'])
        self.assertNotIn('job_id', state)

    def test_polling_running_resume_does_not_announce_a_new_job(self):
        response = self.action('resume')
        (web.STATE/(response.json['id']+'.log')).write_text('Slurmy job ID: test-job\n')
        state = self.client.get('/api/operations/'+response.json['id']).json
        self.assertEqual(state['state'], 'running')
        self.assertNotIn('job_id', state)
        self.assertIn('Resume', state['phase'])

    def test_resume_does_not_block_stop(self):
        self.assertEqual(self.action('resume').status_code, 202)
        self.assertEqual(self.action('stop').status_code, 202)

    def delete(self, **options):
        return self.client.post('/api/jobs/test-job/delete', json=options, headers=self.headers)

    def test_delete_unfinished_job_starts_background_stop_and_delete(self):
        response = self.delete(delete_local=True)
        self.assertEqual(response.status_code, 202)
        self.assertEqual(response.json['label'], 'delete')
        with patch.object(web.subprocess, 'run', return_value=SimpleNamespace(returncode=0)) as run:
            web.threading.Thread.call_args.kwargs['target']()
        argv = run.call_args.args[0]
        self.assertIn('delete', argv)
        self.assertIn('--delete-local', argv)
        state = self.client.get('/api/operations/'+response.json['id']).json
        self.assertEqual(state['state'], 'done')
        self.assertEqual(state['phase'], 'Stop and delete job')

    def test_repeated_delete_joins_existing_operation(self):
        first, second = self.delete(), self.delete()
        self.assertEqual(first.status_code, 202)
        self.assertEqual(first.json['id'], second.json['id'])
        self.assertEqual(web.threading.Thread.call_count, 1)

    def test_delete_waits_for_stop_and_blocks_resume(self):
        stopping = self.action('stop')
        deleting = self.delete()
        self.assertEqual(deleting.status_code, 202)
        self.assertEqual(self.action('resume').status_code, 409)
        # Complete the preceding Stop before executing the Delete worker.
        worker = web.threading.Thread.call_args.kwargs['target']
        with patch.object(web.subprocess, 'run', return_value=SimpleNamespace(returncode=0)):
            # Operation objects are returned by reference through the worker closure.
            stop_worker = web.threading.Thread.call_args_list[0].kwargs['target']
            stop_worker()
            worker()
        state = self.client.get('/api/operations/'+deleting.json['id']).json
        self.assertEqual(state['state'], 'done')
        self.assertIn('Waiting for stop', state['log'])


if __name__ == '__main__':
    unittest.main()
