"""Array capacity decisions against isolated state and mocked scheduler commands."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import contextlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch

import submit_coordinator as coordinator


class ArrayCoordinatorTests(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.home = Path(folder.name)
        self.jobs = self.home / 'slurmy/jobs'
        self.jobs.mkdir(parents=True)
        self.leases = self.home / 'slurmy/submitters'
        self.leases.mkdir()
        self.calls = []
        self.queue = []
        self.limit = 50
        self.replies = []
        self.now = time.time()
        for mock in (
            patch.object(Path, 'home', return_value=self.home),
            patch.dict(coordinator.os.environ, {'USER': 'alice', 'SLURMY_ACCOUNT': 'research',
                                               'SLURMY_PARTITION': 'CPU'}, clear=False),
            patch.object(coordinator.time, 'time', return_value=self.now),
            patch.object(coordinator, 'command', side_effect=self.command),
        ):
            mock.start()
            self.addCleanup(mock.stop)

    def command(self, argv, timeout=20):
        self.calls.append(list(argv))
        if argv[0] == 'squeue':
            return subprocess.CompletedProcess(argv, 0, '\n'.join(self.queue), '')
        if argv[0] == 'sacctmgr':
            output = '' if self.limit is None else f'alice|research|CPU|{self.limit}|\n'
            return subprocess.CompletedProcess(argv, 0, output, '')
        if argv[0] == 'sbatch':
            reply = self.replies.pop(0) if self.replies else (0, '12345;cluster\n', '')
            if isinstance(reply, Exception):
                raise reply
            return subprocess.CompletedProcess(argv, *reply)
        raise AssertionError(argv)

    def queued(self, name, count, first=0):
        directory = self.jobs / name if name else Path('/outside/work')
        # squeue -r supplies one row for every array element, even for one parent.
        return [f'900_{i}|{directory}|public v2 arbitrary comment' for i in range(first, first+count)]

    def lease(self, name, requested, account='research', age=0):
        (self.leases / f'{name}.json').write_text(json.dumps(dict(
            job_id=name, requested=requested, account=account, updated=self.now-age)))

    def attempt(self, requested=10, array='0-9'):
        argv = ['submit_coordinator.py', 'attempt', 'current', str(requested), '--',
                '--parsable', '--array='+array, '--account=research', '--partition=CPU', 'batch.sh']
        output = io.StringIO()
        with patch.object(coordinator.sys, 'argv', argv), contextlib.redirect_stdout(output):
            result = coordinator.main()
        return result, output.getvalue().strip()

    def submitted_arrays(self):
        return [next(option for option in call if option.startswith('--array='))
                for call in self.calls if call[0] == 'sbatch']

    def test_array_size_uses_live_target_minus_current_and_account_headroom(self):
        cases = [
            # requested window, current elements, outside jobs, ceiling, accepted
            (10, 3, 0, 50, 7),
            (10, 0, 18, 20, 2),
            (10, 3, 14, 20, 3),
            (20, 0, 0, 50, 10),  # never extend beyond the requested 0-9 array
        ]
        for requested, current, outside, ceiling, accepted in cases:
            with self.subTest(current=current, outside=outside, ceiling=ceiling):
                self.calls.clear()
                self.limit = ceiling
                # Remove policy state between scenarios so each reads its own ceiling.
                for path in (self.home / 'slurmy').glob('submit-capacity-*.json'):
                    path.unlink()
                self.queue = self.queued('current', current) + self.queued(None, outside, 100)
                code, output = self.attempt(requested)
                self.assertEqual(code, 0)
                self.assertTrue(output.startswith(f'ACCEPTED|12345|accepted={accepted}; '), output)
                self.assertEqual(self.submitted_arrays(), [f'--array=0-{accepted-1}'])

    def test_other_own_jobs_without_submitter_reserve_account_capacity(self):
        self.limit = 12
        self.queue = self.queued('current', 2) + self.queued('parked', 7, 20)
        code, output = self.attempt()
        self.assertEqual(code, 0)
        self.assertIn('accepted=3; 5/5 fair share; 12 user jobs; submit limit 12', output)
        self.assertEqual(self.submitted_arrays(), ['--array=0-2'])

    def test_fairshare_uses_active_leases_and_redistributes_small_demands(self):
        self.limit = 12
        self.lease('peer', 10)
        self.lease('small', 2)
        self.lease('foreign', 100, account='another')
        self.lease('expired', 100, age=coordinator.LEASE_SECONDS+1)
        self.queue = self.queued('current', 2) + self.queued('peer', 3, 20)
        code, output = self.attempt()
        self.assertEqual(code, 0)
        self.assertIn('accepted=3; 5/5 fair share; 8 user jobs', output)
        self.assertEqual(self.submitted_arrays(), ['--array=0-2'])
        self.assertFalse((self.leases / 'expired.json').exists())

    def test_queue_snapshot_counts_array_elements_and_controller_slots(self):
        self.queue = self.queued('current', 4) + self.queued('peer', 2, 10) + self.queued(None, 3, 20)
        self.queue.append(f'999|{self.jobs / "current"}|slurmy-controller:v1')
        self.assertEqual(coordinator.queue_snapshot('alice', self.jobs, 'research'),
                         (10, 4, {'current':4, 'peer':2}))
        self.assertEqual(self.calls, [['squeue','-h','-r','-u','alice','-A','research','-o','%i|%Z|%k']])

    def test_definite_submit_limit_rejection_halves_array_down_to_one(self):
        self.replies = [(1, '', 'AssocMaxSubmitJobLimit')] * 3 + [(0, '12345\n', '')]
        code, output = self.attempt()
        self.assertEqual(code, 0)
        self.assertEqual(self.submitted_arrays(), ['--array=0-9','--array=0-4','--array=0-1','--array=0-0'])
        self.assertTrue(output.startswith('ACCEPTED|12345|accepted=1; '), output)

    def test_final_limit_rejection_at_one_waits_and_records_observed_ceiling(self):
        self.queue = self.queued(None, 1)
        self.replies = [(1, '', 'QOSMaxSubmitJobPerUserLimit')] * 4
        code, output = self.attempt()
        self.assertEqual(code, 0)
        self.assertTrue(output.startswith('WAIT|SLURM rejected submission at 1 user jobs; observed ceiling 1'), output)
        self.assertEqual(self.submitted_arrays(), ['--array=0-9','--array=0-4','--array=0-1','--array=0-0'])
        record = json.loads(next((self.home / 'slurmy').glob('submit-capacity-*.json')).read_text())
        self.assertEqual(record['observed'], 1)

    def test_timeout_is_ambiguous_and_never_resubmitted(self):
        self.replies = [subprocess.TimeoutExpired('sbatch', 60)]
        code, output = self.attempt()
        self.assertEqual(code, 2)
        self.assertTrue(output.startswith('ERROR|Could not confirm sbatch acceptance:'), output)
        self.assertIn('Inspect SLURM before retrying', output)
        self.assertEqual(self.submitted_arrays(), ['--array=0-9'])

    def test_unknown_rejection_does_not_retry(self):
        self.replies = [(1, '', 'Invalid account')]
        code, output = self.attempt()
        self.assertEqual(code, 2)
        self.assertTrue(output.startswith('ERROR|sbatch rejected submission:'), output)
        self.assertEqual(self.submitted_arrays(), ['--array=0-9'])

    def test_full_account_or_reached_target_waits_without_any_submit(self):
        for own, outside in [(0, 10), (10, 0)]:
            with self.subTest(own=own, outside=outside):
                self.calls.clear()
                self.limit = 10
                self.queue = self.queued('current', own) + self.queued(None, outside, 20)
                code, output = self.attempt()
                self.assertEqual(code, 0)
                self.assertTrue(output.startswith('WAIT|'), output)
                self.assertEqual(self.submitted_arrays(), [])

    def test_trimmed_prefix_preserves_nonzero_start(self):
        self.queue = self.queued('current', 8)
        code, output = self.attempt(array='2-5')
        self.assertEqual(code, 0)
        self.assertTrue(output.startswith('ACCEPTED|12345|accepted=2; '), output)
        self.assertEqual(self.submitted_arrays(), ['--array=2-3'])

    def test_array_range_parser_accepts_contiguous_only_and_rejects_duplicates(self):
        self.assertEqual(coordinator.array_request(['--parsable','--array=0-9','batch.sh']), (1,0,10))
        self.assertEqual(coordinator.array_request(['--array=2-5']), (0,2,4))
        self.assertEqual(coordinator.array_request(['--array=0-0']), (0,0,1))
        self.assertEqual(coordinator.array_request(['batch.sh']), (None,0,1))
        for options in (['--array=1,3'], ['--array=0-9%2'], ['--array=0-9:2'],
                        ['--array=unknown'], ['--array=4-2'], ['--array=-1-3'],
                        ['--array=0-9','--array=0-9']):
            with self.subTest(options=options), self.assertRaises(ValueError):
                coordinator.array_request(options)
