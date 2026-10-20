"""Workflow progress must describe this submission, not old prepared inputs."""
import json
import os
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from slurmy_web import app as web


class DispatchProgressTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.workflow = root / 'workflow'
        self.workflow.mkdir()
        (self.workflow / 'Makefile').touch()
        self.metadata = self.workflow / 'submit.sh.files' / 'metadata.json'
        self.metadata.parent.mkdir()
        self.metadata.write_text(json.dumps(dict(task_count=26990)))
        os.utime(self.metadata, (1, 1))
        for target, value in [('STATE', root / 'state'), ('workflows.USER_RUNS', root),
                              ('workflows.migrate_user_runs', lambda: None),
                              ('threading.Thread', unittest.mock.Mock())]:
            patcher = patch('slurmy_web.app.' + target, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        self.app = web.create_app()
        self.app.config['COLLECTOR'] = lambda *args: SimpleNamespace(
            fetch=lambda *args: SimpleNamespace(jobs=[], fetched_epoch=123))
        self.client = self.app.test_client()
        with self.client.session_transaction() as session:
            session['csrf'] = 'test'

    def start(self, action='prepare-submit'):
        response = self.client.post('/api/workflow/action',
            query_string={'directory': str(self.workflow)}, json={'action': action},
            headers={'X-CSRF-Token': 'test'})
        self.assertEqual(response.status_code, 202, response.json)
        return response.json['id']

    def jobs(self):
        response = self.client.get('/api/jobs', query_string={'directory': str(self.workflow)})
        self.assertEqual(response.status_code, 200, response.json)
        return response.json['jobs']

    def test_prepare_ignores_old_count_and_refreshes_after_generation(self):
        self.start()
        self.assertEqual(self.jobs()[0]['total'], 0)
        self.metadata.write_text(json.dumps(dict(task_count=2858)))
        self.assertEqual(self.jobs()[0]['total'], 2858)

    def test_submit_only_uses_existing_prepared_count(self):
        self.start('submit')
        self.assertEqual(self.jobs()[0]['total'], 26990)

    def test_unchanged_prepared_inputs_become_known_when_submission_starts(self):
        operation = self.start()
        self.assertEqual(self.jobs()[0]['total'], 0)
        (web.STATE / (operation + '.log')).write_text('Submission phase: Building dependencies on cluster compute nodes\n')
        self.assertEqual(self.jobs()[0]['total'], 26990)

    def test_running_submit_promotes_real_id_before_worker_finishes(self):
        operation = self.start()
        self.metadata.write_text(json.dumps(dict(task_count=2858)))
        (web.STATE / (operation + '.log')).write_text('Slurmy job ID: current-job_1234567890_12\n')
        jobs = self.jobs()
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0]['id'], 'current-job_1234567890_12')
        self.assertFalse(jobs[0]['provisional'])
        self.assertEqual(jobs[0]['total'], 2858)
        self.assertEqual(self.jobs(), jobs)


if __name__ == '__main__':
    unittest.main()
