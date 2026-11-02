"""Failed submissions expose a bounded controller log in history and job data."""
import fcntl
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from slurmy_web.monitor import REMOTE_SNAPSHOT_SCRIPT, parse_snapshot
from slurmy_web.app import summary


class SubmissionDiagnosticTests(unittest.TestCase):
    def test_zero_batch_failure_includes_actual_controller_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/broken';job.mkdir(parents=True)
            (job/'metadata.json').write_text('{"task_count":2858,"batch_count":64,"planned_batch_tasks":[]}')
            (job/'submission.state').write_text('failed 0 64\n')
            (job/'submission-controller.log').write_text('old output\n'*2000+'ERROR|sbatch: invalid partition specified: bad-partition\n')
            lock=job/'.submit-controller.lock';lock.touch()
            tools=root/'tools';tools.mkdir()
            for name in ('squeue','sacct'):
                tool=tools/name;tool.write_text('#!/bin/sh\nexit 0\n');tool.chmod(0o755)
            def fetch():
                result=subprocess.run(['bash','-s','--','broken'],input=REMOTE_SNAPSHOT_SCRIPT.encode(),capture_output=True,
                                      env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']},check=True)
                return parse_snapshot('host',result.stdout).jobs[0]
            current=fetch()
            self.assertEqual(current.state,'INCOMPLETE')
            self.assertTrue(current.can_resume)
            self.assertLessEqual(len(current.controller_log.encode()),8192)
            self.assertIn('bad-partition',current.attention)
            self.assertIn('bad-partition',summary(current)['controller_log'])
            # A successful restart must not display diagnostics from an old attempt.
            (job/'submission.state').write_text('submitting 0 64\n')
            with lock.open() as stream:
                fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
                restarted=fetch()
                self.assertEqual(restarted.state,'SUBMITTING')
                self.assertEqual(restarted.controller_log,'')
            (job/'submission.state').write_text('stopped 0 64\n')
            self.assertEqual(fetch().controller_log,'')

    def test_failed_startup_is_diagnostic_even_if_controller_never_created_its_lock(self):
        fields=['REMOTE_HOME','/jobs','DETAIL','broken','JOB','broken','1',
                'METADATA','broken','{"task_count":2858,"batch_count":64}',
                'SUBMISSION_STATE','broken','failed 0 64',
                'CONTROLLER_LOG','broken','env: bash: No such file or directory\n',
                'SQUEUE','','SACCT','']
        job=parse_snapshot('host',('\0'.join(fields)+'\0').encode()).jobs[0]
        self.assertEqual(job.state,'INCOMPLETE')
        self.assertIn('No such file',job.attention)
