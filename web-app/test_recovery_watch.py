"""Automatic recovery is based on missing workers, not unchanged counts."""
import unittest
from slurmy_web.monitor import JobSnapshot
from slurmy_web.recovery_watch import RecoveryWatch


class RecoveryWatchTests(unittest.TestCase):
    def job(self, **values):
        return JobSnapshot('job', '/jobs/job', 0, metadata={'task_count': 10},
                           submission_state='submitting', controller_state='inactive',
                           queue_available=True, **values)

    def test_idle_controller_loss_is_retried_after_confirmation(self):
        watch = RecoveryWatch()
        job = self.job()
        self.assertEqual(watch.observe('host', [job], now=0), [])
        self.assertEqual(watch.observe('host', [job], now=14), [])
        self.assertEqual(watch.observe('host', [job], now=15), [job])
        self.assertEqual(watch.observe('host', [job], now=30), [])
        self.assertEqual(watch.observe('host', [job], now=75), [job])
        self.assertEqual(watch.observe('host', [job], now=135), [job])
        self.assertEqual(watch.observe('host', [job], now=300), [])
        job.completed = 1
        self.assertEqual(watch.observe('host', [job], now=301), [job])

    def test_running_controller_queued_tasks_and_unknown_queue_are_not_idle(self):
        watch = RecoveryWatch(idle_seconds=0)
        job = self.job()
        job.controller_state = 'active'
        self.assertEqual(watch.observe('host', [job], now=0), [])
        self.assertEqual(watch.observe('host', [job], now=10000), [])
        job.controller_state = 'inactive'
        job.queue_available = False
        self.assertEqual(watch.observe('host', [job], now=10001), [])
        job.queue_available = True
        # Even pending tasks must prevent recovery; no restart merely for flat counts.
        job.slurm = [type('Record', (), {'source':'queue','state':'PENDING'})()]
        self.assertEqual(watch.observe('host', [job], now=10002), [])

    def test_explicit_stop_and_historical_failures_are_not_revived(self):
        watch = RecoveryWatch(idle_seconds=0)
        job = self.job()
        watch.stop('host', 'job')
        self.assertEqual(watch.observe('host', [job], now=100), [])
        watch.resume('host', 'job')
        self.assertEqual(watch.observe('host', [job], now=101), [job])
        job.submission_state = 'failed'
        self.assertEqual(RecoveryWatch(idle_seconds=0).observe('host', [job]), [])
        job.submission_state = 'stopped'
        self.assertEqual(watch.observe('host', [job], now=500), [])

    def test_monitor_request_exposes_automatic_operation_for_progress_toast(self):
        from pathlib import Path
        from types import SimpleNamespace
        from unittest.mock import patch
        import tempfile
        from slurmy_web import app as web
        job = self.job()
        job.controller_state = 'active'
        value = SimpleNamespace(jobs=[job], fetched_epoch=100, find_job=lambda key: job)
        with tempfile.TemporaryDirectory() as folder, \
             patch.object(web.workflows, 'migrate_user_runs'), \
             patch.object(web, 'STATE', Path(folder)), \
             patch.object(web.threading, 'Thread') as thread:
            app = web.create_app()
            app.config['COLLECTOR'] = lambda *args: SimpleNamespace(fetch=lambda *args: value)
            app.config['RECOVERY_WATCH'].idle_seconds = 0
            client = app.test_client()
            self.assertEqual(client.get('/api/jobs/job?fresh=1').status_code, 200)
            job.controller_state = 'inactive'
            response = client.get('/api/jobs/job?fresh=1')
            self.assertEqual(response.status_code, 200)
            operation = response.json['recovery_operation']
            self.assertTrue(operation)
            self.assertTrue(client.get('/api/operations/' + operation).json['automatic'])
            self.assertEqual(client.get('/api/jobs/job?fresh=1').json['recovery_operation'], operation)
            self.assertEqual(thread.call_count, 1)
