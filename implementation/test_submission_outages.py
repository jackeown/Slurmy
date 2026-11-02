"""Read-only scheduler failures pause submission without risking duplicate work."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import submit_coordinator as coordinator


class SubmissionOutageTests(unittest.TestCase):
    def test_queue_outage_waits_without_submitting_then_recovers(self):
        failures = [subprocess.CompletedProcess([], 1, '', 'slurmctld unavailable'),
                    subprocess.TimeoutExpired('squeue', 20), OSError('connection reset')]
        for failure in failures:
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as folder:
                def unavailable(argv, **kwargs):
                    self.assertEqual(argv[0], 'squeue')
                    if isinstance(failure, Exception):
                        raise failure
                    return failure
                argv = ['submit_coordinator.py', 'attempt', 'test-job', '32', '--', 'batch.sh']
                with patch.object(Path, 'home', return_value=Path(folder)), patch.object(sys, 'argv', argv), patch.dict(os.environ, USER='tester'), patch.object(coordinator, 'command', side_effect=unavailable), patch('builtins.print') as output:
                    self.assertEqual(coordinator.main(), 0)
                    self.assertTrue(output.call_args.args[0].startswith('WAIT|Cannot read SLURM queue'))
                replies = [subprocess.CompletedProcess([], 0, '', ''),
                           subprocess.CompletedProcess([], 0, '12345\n', '')]
                with patch.object(Path, 'home', return_value=Path(folder)), patch.object(sys, 'argv', argv), patch.dict(os.environ, USER='tester'), patch.object(coordinator, 'reported_limit', return_value=None), patch.object(coordinator, 'command', side_effect=replies) as commands, patch('builtins.print') as output:
                    self.assertEqual(coordinator.main(), 0)
                    self.assertEqual([c.args[0][0] for c in commands.call_args_list], ['squeue', 'sbatch'])
                    self.assertTrue(output.call_args.args[0].startswith('ACCEPTED|12345|'))

    def test_early_input_failure_records_phase_and_failed_state(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            job = root / 'slurmy/jobs/test-job'
            job.mkdir(parents=True)
            result = subprocess.run(['bash', str(Path(__file__).parent / 'templates/remote_submit.sh'), 'test-job', 'CPU-amd', '32', 'test'], env={**os.environ, 'HOME': folder}, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Submission controller failed during initializing committed inputs', result.stderr)
            self.assertTrue((job / 'submission.state').read_text().startswith('failed 0 0'))
