"""A stale state file must not keep an abandoned job looking active."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from slurmy_web.monitor import JobSnapshot, REMOTE_SNAPSHOT_SCRIPT, parse_snapshot


class SubmissionLivenessTests(unittest.TestCase):
    def test_interrupted_submission_is_incomplete_and_resumable(self):
        job = JobSnapshot('job', '/jobs/job', 1,
                          metadata={'task_count': 100, 'batch_count': 10}, completed=20,
                          submission_state='submitting', controller_state='inactive')
        self.assertEqual(job.state, 'INCOMPLETE')
        self.assertTrue(job.can_resume)
        self.assertIn('controller stopped', job.attention)
        self.assertGreater(job.issue_count, 0)
        job.controller_state = 'active'
        self.assertEqual(job.state, 'SUBMITTING')
        self.assertFalse(job.can_resume)
        self.assertEqual(job.attention, '')
        job.completed = 100
        self.assertTrue(job.state.startswith('DONE'))
        self.assertFalse(job.can_resume)
        self.assertEqual(job.attention, '')

    def test_lock_liveness_changes_even_when_overview_is_cached(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            job = root / 'slurmy/jobs/job'
            job.mkdir(parents=True)
            (job / 'metadata.json').write_text('{"task_count":100,"batch_count":10}')
            (job / 'submission.state').write_text('submitting 2 10\n')
            lock = job / '.submit-controller.lock'
            lock.touch()
            tools = root / 'tools'
            tools.mkdir()
            for name in ('squeue', 'sacct'):
                tool = tools / name
                tool.write_text('#!/bin/sh\nexit 0\n')
                tool.chmod(0o755)
            env = {**os.environ, 'HOME': folder, 'PATH': str(tools) + os.pathsep + os.environ['PATH']}
            def snapshot():
                result = subprocess.run(['bash', '-s', '--', '-'], env=env,
                                        input=REMOTE_SNAPSHOT_SCRIPT.encode(), capture_output=True, check=True)
                return parse_snapshot('test', result.stdout).jobs[0]
            self.assertEqual(snapshot().controller_state, 'inactive')
            cache = root / '.cache/slurmy/history-v2/job'
            original_mtime = cache.stat().st_mtime_ns
            import fcntl
            with lock.open() as stream:
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.assertEqual(snapshot().controller_state, 'active')
            self.assertEqual(snapshot().controller_state, 'inactive')
            self.assertEqual(cache.stat().st_mtime_ns, original_mtime)
