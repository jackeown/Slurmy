"""The inspector preserves native timestamps and keeps raw streams separate."""
import csv
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from slurmy_web.monitor import REMOTE_TABLE_ANALYSIS_SCRIPT, REMOTE_TASK_OUTPUT_SCRIPT, parse_task_output


class OutputStreamTests(unittest.TestCase):
    def test_inspector_reads_native_combined_output_without_rewriting_raw_stdout(self):
        with tempfile.TemporaryDirectory() as directory:
            home=Path(directory);results=home/'slurmy/jobs/test/results';results.mkdir(parents=True)
            stem='task_000000000_abcdef'
            native=b'0.00/0.01\tearly stderr\n0.03/0.50\tstdout\n0.03/0.51\tEOF\n'
            with tarfile.open(results/'result.tar.gz','w:gz') as archive:
                for suffix,content in [('solver.log',b'stdout\n'),('solver-stderr.log',b'early stderr\n'),('solver-combined.log',native)]:
                    member=tarfile.TarInfo(stem+'.'+suffix);member.size=len(content);archive.addfile(member,io.BytesIO(content))
            output=subprocess.run(['bash','-c',REMOTE_TASK_OUTPUT_SCRIPT,'test','test','result.tar.gz','0','abcdef'],env={**os.environ,'HOME':directory},capture_output=True,check=True)
            streams=parse_task_output('test',0,'result.tar.gz',output.stdout).streams
            self.assertEqual(streams['solver_combined'].content,native.decode())
            self.assertEqual(streams['solver'].content,'stdout\n')
            self.assertEqual(streams['solver_stderr'].content,'early stderr\n')

    def test_inspector_reads_analysis_for_the_selected_attempt(self):
        with tempfile.TemporaryDirectory() as directory:
            results = Path(directory)/'slurmy/jobs/test/results'
            results.mkdir(parents=True)
            archive_name = 'batch_12_task_000000034_abcdef_789_1234567890.tar.gz'
            with tarfile.open(results/archive_name, 'w:gz') as archive:
                member = tarfile.TarInfo('./task_000000034_abcdef.solver.log')
                member.size = 6
                archive.addfile(member, io.BytesIO(b'stdout'))
            metrics = {'proof_steps': 0, 'verified': False, 'notes': 'line 1,\nline 2', 'optional': None}
            csv_path = results/'batch_000012_task_000000034.csv'
            for attempt, expected in ((archive_name, metrics), ('new-attempt.tar.gz', {})):
                with csv_path.open('w', newline='', encoding='utf-8') as stream:
                    writer = csv.DictWriter(stream, fieldnames=['task_id', 'task_key', 'archive', 'analysis'])
                    writer.writeheader()
                    writer.writerow(dict(task_id=34, task_key='abcdef', archive=attempt, analysis=json.dumps(metrics)))
                output = subprocess.run(['bash', '-c', REMOTE_TASK_OUTPUT_SCRIPT, 'test', 'test',
                                         archive_name, '34', 'abcdef'], env={**os.environ, 'HOME': directory},
                                        capture_output=True, check=True)
                result = parse_task_output('test', 34, archive_name, output.stdout)
                self.assertEqual(result.analysis, expected)
                self.assertEqual(result.streams['solver'].content, 'stdout')
                table = subprocess.run([sys.executable, '-c', REMOTE_TABLE_ANALYSIS_SCRIPT, 'test'],
                                       input=json.dumps(dict(tasks=[(34, archive_name, 'abcdef')],
                                                             columns=['proof_steps', 'verified', 'missing'])),
                                       env={**os.environ, 'HOME': directory}, capture_output=True, text=True, check=True)
                values = {'34': {'proof_steps': 0, 'verified': False}} if expected else {}
                self.assertEqual(json.loads(table.stdout), {'columns': list(expected), 'rows': values})
