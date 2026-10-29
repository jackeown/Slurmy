"""Exercise the real remote overview protocol and atomic cache invalidation."""
import csv
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from slurmy_web.monitor import REMOTE_SNAPSHOT_SCRIPT, parse_snapshot


class HistoryCacheTests(unittest.TestCase):
    def test_overview_cache_reuses_and_invalidates_without_task_details(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            job=root/'slurmy/jobs/example'
            results=job/'results'
            results.mkdir(parents=True)
            (job/'metadata.json').write_text('{"task_count":1}')
            (job/'progress').mkdir()
            # A malformed per-pair progress file must never be read by overview.
            (job/'progress/task_0.csv').mkdir()
            def publish(status):
                output=io.StringIO()
                fields=['']*17
                fields[:5]=['0','true',status,'0','1']
                fields[15]='Theorem' if status=='ok' else 'Timeout'
                csv.writer(output,quoting=csv.QUOTE_ALL).writerow(fields)
                temporary=results/'new.tmp'
                temporary.write_text(output.getvalue())
                temporary.replace(results/'batch_000000_task_000000000.csv')
            publish('ok')
            commands=root/'bin'
            commands.mkdir()
            for command in ('squeue','sacct'):
                executable=commands/command
                executable.write_text('#!/bin/sh\nprintf "query\\n" >> "$HOME/'+command+'-queries"\n')
                executable.chmod(0o755)
            env={**os.environ,'HOME':folder,'PATH':str(commands)+os.pathsep+os.environ['PATH']}
            def fetch():
                process=subprocess.run(['bash','-s','--','-'],input=REMOTE_SNAPSHOT_SCRIPT.encode(),
                                       capture_output=True,env=env,timeout=10,check=True)
                self.assertNotIn(b'CALL_PROGRESS',process.stdout)
                self.assertNotIn(b'MANIFEST',process.stdout)
                self.assertNotIn(b'PROGRESS_CSV',process.stdout)
                return parse_snapshot('test',process.stdout).jobs[0]
            self.assertEqual(fetch().status_counts,{'ok':1})
            cached=root/'.cache/slurmy/history-v2/example'
            timestamp=cached.stat().st_mtime_ns
            self.assertEqual(fetch().status_counts,{'ok':1})
            self.assertEqual(cached.stat().st_mtime_ns,timestamp)
            for command in ('squeue','sacct'):
                self.assertEqual(len((root/(command+'-queries')).read_text().splitlines()),2)
            publish('time-limit')
            self.assertEqual(fetch().status_counts,{'time-limit':1})
            (job/'metadata.json').write_text('{"task_count":2}')
            self.assertEqual(fetch().task_count,2)
            cached.write_bytes(b'broken cache')
            self.assertEqual(fetch().status_counts,{'time-limit':1})
