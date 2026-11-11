"""A pending cluster controller is live submission, never a solver batch."""
import json
import unittest

from slurmy_web.monitor import parse_snapshot
from slurmy_web.app import naive_eta_seconds


class ManagedControllerTests(unittest.TestCase):
    def test_cluster_view_names_controller_and_does_not_assign_solver_counts(self):
        from slurmy_web.cluster import parse_status
        from test_cluster_status import snapshot
        data = snapshot().replace('slurmy:v1:calls=4:batches=1:batch=0', 'slurmy-controller:v1')
        data = data.replace('101_0|0|', '999_0|0|')  # No solver is running in this allocation.
        controller = parse_status('test', 'CPU-amd', data)['jobs'][0]
        self.assertEqual(controller['name'], 'Submission Controller')
        self.assertTrue(controller['controller'])
        self.assertIsNone(controller['solver_cores'])
        self.assertNotIn('jobpairs', controller)
        self.assertNotIn('batches', controller)

    def test_pending_and_running_controllers_are_live_but_not_solver_parallelism(self):
        for state in ('PENDING', 'RUNNING'):
            with self.subTest(state=state):
                payload = b'REMOTE_HOME\0/home/u/slurmy/jobs\0DETAIL\0job\0JOB\0job\0'+b'1\0'
                payload += b'METADATA\0job\0'+json.dumps({'task_count':3,'batch_count':3}).encode()+b'\0'
                payload += b'SUBMISSION_STATE\0job\0submitting 1 3\0CONTROLLER_STATE\0job\0inactive\0'
                payload += b'CONTROLLER_LOG\0job\0old interrupted controller\0'
                payload += b'SQUEUE\0'+f'101|101|N/A|{state}|0:01|9:59|slurmy:controller|/home/u/slurmy/jobs/job|node1'.encode()+b'\0SACCT\0\0'
                job = parse_snapshot('test', payload).jobs[0]
                self.assertEqual(job.controller_state, 'active')
                self.assertEqual(job.state, 'SUBMITTING')
                self.assertEqual(job.attention, '')
                self.assertEqual(job.controller_log, '')
                self.assertEqual(job.active_records, [])
                self.assertEqual(job.active_batch_ids, set())
                self.assertFalse(job.can_resume)
                self.assertIsNone(naive_eta_seconds(job))
