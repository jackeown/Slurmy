"""Detail polling avoids unrelated jobs and legacy raw-directory scans."""
import csv
import gzip
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from slurmy_web.monitor import REMOTE_SNAPSHOT_SCRIPT,RemoteCollector,parse_snapshot


class MonitorFastPathTests(unittest.TestCase):
    def test_detail_only_reads_selected_job_and_compact_formats(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/selected';(job/'results/index').mkdir(parents=True)
            (job/'results/index/.ready').touch()
            (job/'metadata.json').write_text('{"task_count":1,"batch_size":1,"planned_batch_tasks":[[0]]}')
            (job/'call.sh').write_text('# The watcher is authoritative.\n')
            (job/'batches').mkdir();(job/'batches/batch_000000.sh').touch()
            (job/'progress').mkdir();(job/'progress/batch_000000.csv').write_text('"running","0","1","2","0"\n')
            (job/'manifest.jsonl').write_text('{"task_id":0,"solver":"S","problem":"/example.p"}\n')
            fields=['']*17;fields[:4]=['0','true','solver-error','143'];fields[14]='batch_0_task_0.tar.gz'
            with (job/'results/index/batch_000000.csv').open('w') as stream:csv.writer(stream,quoting=csv.QUOTE_ALL).writerow(fields)
            for name in ('batch_0.tsv','chunk_0.jsonl'):(job/'results'/name).write_text('legacy data must not be read')
            (job/'results/batch_0_task_0.tar.gz').touch()
            other=root/'slurmy/jobs/other';other.mkdir();(other/'metadata.json').write_text('{"task_count":9999}')
            tools=root/'tools';tools.mkdir()
            for name in ('squeue','sacct'):
                path=tools/name;path.write_text('#!/bin/sh\nexit 0\n');path.chmod(0o755)
            tar=tools/'tar';tar.write_text('#!/bin/sh\necho scanned >> "$HOME/archive-scans"\n');tar.chmod(0o755)
            process=subprocess.run(['bash','-s','--','selected'],input=REMOTE_SNAPSHOT_SCRIPT.encode(),capture_output=True,
                                   env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']},check=True,timeout=5)
            snapshot=parse_snapshot('test',process.stdout)
            self.assertEqual([item.job_id for item in snapshot.jobs],['selected'])
            self.assertEqual(snapshot.jobs[0].tasks[0].problem,'/example.p')
            self.assertEqual(snapshot.jobs[0].progress[0].task_id,0)
            self.assertNotIn(b'RESULTS_JSONL',process.stdout);self.assertNotIn(b'RESULTS_TSV',process.stdout)
            self.assertFalse((root/'archive-scans').exists())

    def test_transport_decompresses_and_rejects_incomplete_responses(self):
        # Build the timestamp separately so Python does not treat it as octal.
        payload=b'REMOTE_HOME\0/jobs\0DETAIL\0selected\0JOB\0selected\0'+b'10\0SQUEUE\0\0SACCT\0\0'
        with patch('slurmy_web.monitor.subprocess.run',return_value=SimpleNamespace(returncode=0,stdout=gzip.compress(payload),stderr=b'')) as run:
            result=RemoteCollector('test',10).fetch('selected')
            self.assertEqual(result.jobs[0].job_id,'selected')
            self.assertIn('gzip -1',run.call_args.args[0][-1])
        with patch('slurmy_web.monitor.subprocess.run',return_value=SimpleNamespace(returncode=0,stdout=b'broken',stderr=b'')):
            with self.assertRaisesRegex(RuntimeError,'Incomplete compressed'):RemoteCollector('test',10).fetch('selected')
