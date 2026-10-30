"""Profiling uses one bounded read and reuses captured data for local work."""
import gzip
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import profile_monitor as profiler


class MonitorProfilingTests(unittest.TestCase):
    def test_remote_stages_use_remote_differences_without_clock_sync_assumptions(self):
        stages = profiler.remote_stages('warning\nPROFILE|begin|1000.0\nPROFILE|rows|1000.25\nPROFILE|done|1002.0\n')
        self.assertEqual(stages, [{'stage':'begin', 'seconds':.25}, {'stage':'rows', 'seconds':1.75}])
        instrumented = profiler.instrument(profiler.REMOTE_SNAPSHOT_SCRIPT)
        self.assertIn('PROFILE|progress|', instrumented)
        self.assertIn('>&2', instrumented)
        with self.assertRaisesRegex(ValueError, 'marker missing'):
            profiler.instrument('unrelated script')

    def test_one_ssh_request_even_with_saved_payload_and_python_profile(self):
        fields = ['REMOTE_HOME','/jobs','DETAIL','job','JOB','job','1','METADATA','job',
                  '{"task_count":1}','SQUEUE','','SACCT','']
        payload = ('\0'.join(fields)+'\0').encode()
        result = SimpleNamespace(returncode=0, stdout=gzip.compress(payload),
                                 stderr=b'PROFILE|begin|1\nPROFILE|done|1.5\n')
        with tempfile.TemporaryDirectory() as folder, patch.object(profiler.subprocess, 'run', return_value=result) as remote:
            saved, stats = Path(folder)/'payload.gz', Path(folder)/'parse.prof'
            report = profiler.profile('host', 'job', True, saved, stats)
            self.assertEqual(remote.call_count, 1)
            self.assertEqual(gzip.decompress(saved.read_bytes()), payload)
            self.assertTrue(stats.is_file())
            self.assertEqual(report['solver_problem_pairs'], 1)
            self.assertEqual(report['uncompressed_bytes'], len(payload))
            self.assertIn('timeout -k 5 45', remote.call_args.args[0][-1])
            self.assertEqual(remote.call_args.kwargs['timeout'], 55)
            self.assertNotIn('payload', report)

    def test_bad_ids_are_rejected_before_any_remote_command(self):
        with patch.object(profiler.subprocess, 'run') as remote:
            for host, job in [('host','../job'), ('-oBad','job')]:
                with self.assertRaises(ValueError):
                    profiler.profile(host, job)
            remote.assert_not_called()
