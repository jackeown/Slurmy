"""The inspector preserves native timestamps and keeps raw streams separate."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from slurmy_web.monitor import REMOTE_TASK_OUTPUT_SCRIPT, parse_task_output


class OutputStreamTests(unittest.TestCase):
    def test_inspector_reads_native_combined_output_without_rewriting_raw_stdout(self):
        with tempfile.TemporaryDirectory() as directory:
            home=Path(directory);results=home/'slurmy/jobs/test/results';results.mkdir(parents=True)
            stem='task_000000000_abcdef'
            native=b'0.00/0.01\tearly stderr\n0.03/0.50\tstdout\n0.03/0.51\tEOF\n'
            with tarfile.open(results/'result.tar.gz','w:gz') as archive:
                for suffix,content in [('postprocessor.log',b'SZS data status=unsat\n'),('postprocessor-stderr.log',b'parse warning\n'),('solver.log',b'stdout\n'),('solver-stderr.log',b'early stderr\n'),('solver-combined.log',native)]:
                    member=tarfile.TarInfo(stem+'.'+suffix);member.size=len(content);archive.addfile(member,io.BytesIO(content))
            output=subprocess.run(['bash','-c',REMOTE_TASK_OUTPUT_SCRIPT,'test','test','result.tar.gz','0','abcdef'],env={**os.environ,'HOME':directory},capture_output=True,check=True)
            streams=parse_task_output('test',0,'result.tar.gz',output.stdout).streams
            self.assertEqual(streams['postprocessor'].content,'SZS data status=unsat\n')
            self.assertEqual(streams['postprocessor_stderr'].content,'parse warning\n')
            self.assertEqual(streams['solver_combined'].content,native.decode())
            self.assertEqual(streams['solver'].content,'stdout\n')
            self.assertEqual(streams['solver_stderr'].content,'early stderr\n')
