"""Cluster-managed submission continues without login processes or held nodes."""
import os
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import submission_controller as controller
import submit_coordinator

ARGS = ['job', 'CPU-amd', '1', 'job', 'am9i', '0', 'reservation-1', '1']


class ControllerTests(unittest.TestCase):
    def test_controller_only_queue_at_account_limit_fails_instead_of_repeating_forever(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); (root/'slurmy').mkdir()
            (root/'slurmy/submit-capacity.json').write_text(json.dumps({'reported':2}))
            job = root/'job'; job.mkdir()
            with patch.dict(os.environ, SLURM_JOB_ID='101'), \
                    patch.object(controller.Path, 'home', return_value=root), \
                    patch.object(controller, 'submit', return_value='102'), \
                    patch.object(controller.subprocess, 'run', return_value=SimpleNamespace(returncode=75)), \
                    patch.object(controller, 'queue', return_value=[['101',str(job),controller.ROLE], ['102',str(job),controller.ROLE]]), \
                    patch.object(controller, 'command') as command:
                with self.assertRaisesRegex(RuntimeError, 'three account submission slots'):
                    controller.run(job, ARGS)
                command.assert_called_once_with(['scancel','102'])

    def test_launch_returns_after_sbatch_acceptance_and_duplicate_joins_pending_controller(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            with patch.object(controller, 'queue', return_value=[]), \
                    patch.object(controller, 'submit', return_value='101') as submit:
                controller.launch(job, ARGS)
            self.assertEqual((job/'.submission-controller.id').read_text(), '101\n')
            submit.assert_called_once_with(job, ARGS)
            with patch.object(controller, 'queue', return_value=[['101', str(job), controller.ROLE]]), \
                    patch.object(controller, 'submit') as submit:
                controller.launch(job, ARGS)
                submit.assert_not_called()

    def test_full_window_hands_off_to_worker_dependency_and_returns(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            with patch.dict(os.environ, SLURM_JOB_ID='101'), \
                    patch.object(controller, 'submit', return_value='102') as submit, \
                    patch.object(controller.subprocess, 'run', return_value=SimpleNamespace(returncode=75)) as run, \
                    patch.object(controller, 'queue', return_value=[['101', str(job), controller.ROLE],
                        ['102', str(job), controller.ROLE], ['100_0', str(job), 'slurmy:v1:batch=0']]), \
                    patch.object(controller, 'command') as command:
                controller.run(job, ARGS)
            self.assertEqual(submit.call_args.args[1][5], '2')
            self.assertEqual(submit.call_args.args[2], '101')
            command.assert_called_once_with(['scontrol', 'update', 'JobId=102', 'Dependency=afterany:101:100_0'])
            self.assertEqual((job/'.submission-controller.id').read_text(), '102\n')
            self.assertEqual(run.call_args.kwargs['env']['SLURMY_MANAGED_CONTROLLER'], '1')

    def test_completed_failed_and_stopped_waves_cancel_the_unused_successor(self):
        for outcome in ('complete', 'failed', 'stopped'):
            with self.subTest(outcome=outcome), tempfile.TemporaryDirectory() as folder:
                job = Path(folder)
                def worker(*args, **kwargs):
                    if outcome == 'stopped': (job/'.stopped').touch()
                    return SimpleNamespace(returncode=0 if outcome=='complete' else 1)
                with patch.dict(os.environ, SLURM_JOB_ID='101'), \
                        patch.object(controller, 'submit', return_value='102'), \
                        patch.object(controller.subprocess, 'run', side_effect=worker), \
                        patch.object(controller, 'command') as command:
                    if outcome == 'failed':
                        with self.assertRaisesRegex(RuntimeError, 'wave failed'): controller.run(job, ARGS)
                    else: controller.run(job, ARGS)
                    command.assert_called_once_with(['scancel', '102'])

    def test_transient_queue_outage_uses_delayed_allocation_free_retry(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)
            with patch.dict(os.environ, SLURM_JOB_ID='101'), \
                    patch.object(controller, 'submit', return_value='102'), \
                    patch.object(controller.subprocess, 'run', return_value=SimpleNamespace(returncode=75)), \
                    patch.object(controller, 'queue', return_value=[]), \
                    patch.object(controller, 'command') as command:
                controller.run(job, ARGS)
            self.assertIn('StartTime=now+60seconds', command.call_args.args[0])

    def test_controller_sbatch_has_reservation_small_allocation_and_dependencies(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); (root/'slurmy').mkdir(); job=root/'slurmy/jobs/job'
            with patch.object(controller.Path, 'home', return_value=root), \
                    patch.object(controller, 'command', return_value='102;cluster') as command:
                self.assertEqual(controller.submit(job, ARGS, '101'), '102')
            options = command.call_args.args[0]
            for option in ['--reservation=reservation-1', '--cpus-per-task=1', '--mem=256M',
                           '--dependency=afterany:101', '--comment='+controller.ROLE]:
                self.assertIn(option, options)
            self.assertNotIn('--exclusive', options)

    def test_controllers_use_account_capacity_but_not_worker_window(self):
        result = SimpleNamespace(returncode=0, stdout='101|/home/u/slurmy/jobs/job|slurmy-controller:v1\n'
                                 '102|/home/u/slurmy/jobs/job|slurmy-controller:v1\n'
                                 '100_0|/home/u/slurmy/jobs/job|slurmy:v1:batch=0\n')
        with patch.object(submit_coordinator, 'command', return_value=result):
            self.assertEqual(submit_coordinator.queue_snapshot('u', Path('/home/u/slurmy/jobs')), (3, 2, {'job':1}))
