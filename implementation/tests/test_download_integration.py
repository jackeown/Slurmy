"""Exercise the actual download engines through an isolated local SSH transport."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import contextlib
import csv
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'web-app'))
import download
from slurmy import RESULT_COLUMNS


@unittest.skipUnless(shutil.which('rsync') and shutil.which('bash'),'Full download formats require rsync and Bash')
class DownloadIntegrationTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.remote=self.root/'cluster';self.bin=self.root/'bin';self.bin.mkdir()
        self.identifier='download-demo_1795517000_1'
        self.job=self.remote/'slurmy/jobs'/self.identifier;self.job.mkdir(parents=True)
        (self.job/'metadata.json').write_text(json.dumps(dict(job_name='Archive demo (1)',task_count=1,batch_count=1,planned_batch_tasks=[[0]],batch_size=1)))
        definition=dict(task_id=0,solver='Example',problem='/problems/SET002+3.p',command='./solver',cores=1,wc_limit=1,cpu_limit=1,mem_limit=100)
        (self.job/'manifest.jsonl').write_text(json.dumps(definition)+'\n')
        results=self.job/'results';(results/'index').mkdir(parents=True)
        archive='batch_0_task_000000000_abcdef_12345_1795517001.tar.gz'
        with tarfile.open(results/archive,'w:gz') as target:
            for name,content in [('solver','% SZS status Theorem\n'),('solver-stderr',''),('postprocessor','SZS data status=Theorem\n')]:
                payload=content.encode();info=tarfile.TarInfo('task_000000000_abcdef.'+name+'.log');info.size=len(payload);target.addfile(info,io.BytesIO(payload))
        row=dict(task_id=0,complete='true',status='ok',return_code=0,wall_seconds=.1,cpu_seconds=.05,task_key='abcdef',archive=archive,szs_status='Theorem',postprocess_data=json.dumps({'status':'Theorem'}),postprocess_return_code=0,postprocess_status='Theorem')
        with (results/'batch_000000_task_000000000.csv').open('w',newline='') as stream:
            writer=csv.DictWriter(stream,fieldnames=RESULT_COLUMNS);writer.writeheader();writer.writerow(row)
        shutil.copy2(results/'batch_000000_task_000000000.csv',results/'index/batch_000000.csv');(results/'index/.ready').touch()
        executable=self.bin/'ssh'
        executable.write_text('#!'+sys.executable+'\n'+'''import os,sys
args=sys.argv[1:]
while args:
 if args[0] in ['-o','-p','-i','-l','-F','-J']:args=args[2:]
 elif args[0]=='--':args=args[1:];break
 elif args[0].startswith('-'):args=args[1:]
 else:break
assert args.pop(0)=='fixture',args
os.environ['HOME']=os.environ['TEST_CLUSTER_HOME']
os.chdir(os.environ['HOME'])
os.execvp('bash',['bash','-c',' '.join(args)])
''');executable.chmod(0o755)
        self.environment=dict(PATH=str(self.bin)+os.pathsep+os.environ['PATH'],TEST_CLUSTER_HOME=str(self.remote),SLURMY_HOST='fixture')

    def test_real_engines_all_formats_are_silent_and_return_true(self):
        with patch.dict(os.environ,self.environment):
            for mode in download.MODES:
                with self.subTest(mode=mode):
                    output=self.root/mode;stdout,stderr=io.StringIO(),io.StringIO()
                    with contextlib.redirect_stdout(stdout),contextlib.redirect_stderr(stderr):
                        success=download.download('Archive demo (1)',mode,host='fixture',output=output)
                    self.assertTrue(success,mode);self.assertEqual(stdout.getvalue()+stderr.getvalue(),'')
                    if mode=='default':
                        self.assertEqual(len(list(output.glob('*.tgz'))),3)
                        self.assertEqual(len(list(output.glob('*_results-summary.csv'))),1)
                    elif mode=='starexec':self.assertEqual(len(list(output.glob('*.zip'))),2)
                    else:self.assertTrue((output/'solver-problem-pairs').is_dir())

    def run_engine(self, mode, destination, incremental=False):
        argv=[sys.executable,str(ROOT/'implementation/slurmy-sync.py'),self.identifier,'--host','fixture','--output',str(destination)]
        if incremental:argv.append('--incremental')
        if mode!='default':argv.append('--'+mode)
        result=subprocess.run(argv,cwd=self.root,env={**os.environ,**self.environment},capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+'\n'+result.stderr)
        return result

    def summary_rows(self, destination, mode):
        if mode=='starexec':
            import zipfile
            with zipfile.ZipFile(next(destination.glob('*_info.zip'))) as archive:
                name=next(name for name in archive.namelist() if name.endswith('.csv'))
                return list(csv.DictReader(io.StringIO(archive.read(name).decode())))
        with next(destination.glob('*_results-summary.csv')).open() as stream:
            return list(csv.DictReader(stream))

    def test_incremental_exports_only_new_completed_results_and_full_snapshots_reuse_cache(self):
        from download_records import snapshots
        for mode in ('default','starexec'):
            with self.subTest(mode=mode):
                first=self.root/'job-results'/(mode+'-first')
                self.run_engine(mode,first,incremental=True)
                self.assertEqual(len(self.summary_rows(first,mode)),1)
                before={path.name:path.read_bytes() for path in first.iterdir()}
                second=self.root/'job-results'/(mode+'-second')
                result=self.run_engine(mode,second,incremental=True)
                self.assertIn('0 paths updated',result.stdout)
                self.assertEqual(self.summary_rows(second,mode),[])
                if mode=='default':
                    for path in second.glob('*.tgz'):
                        with tarfile.open(path) as archive:self.assertFalse(any(m.isfile() for m in archive))
                full=self.root/'job-results'/(mode+'-full')
                result=self.run_engine(mode,full)
                self.assertIn('0 paths updated',result.stdout)
                self.assertEqual(len(self.summary_rows(full,mode)),1)
                self.assertEqual({path.name:path.read_bytes() for path in first.iterdir()},before)
                self.assertIn(second,snapshots(self.root/'job-results','fixture',self.identifier))
                if mode=='default':self.assertNotEqual(next(first.glob('*.csv')).name,next(full.glob('*.csv')).name)
                else:
                    import zipfile
                    with zipfile.ZipFile(next(first.glob('*_info.zip'))) as old, zipfile.ZipFile(next(full.glob('*_info.zip'))) as new:
                        self.assertNotEqual(old.namelist(),new.namelist())

    def test_debug_full_snapshots_also_reuse_cache(self):
        first=self.root/'debug-first';second=self.root/'debug-second'
        self.run_engine('debug',first)
        result=self.run_engine('debug',second)
        self.assertIn('0 paths updated',result.stdout)
        self.assertTrue((second/'diagnostics/results').is_dir())
        self.assertTrue((first/'solver-problem-pairs').is_dir())

    def test_full_download_sets_baseline_and_new_completion_is_in_next_delta_only(self):
        # The unfinished definition belongs in a full CSV, but never in a delta.
        with (self.job/'manifest.jsonl').open('a') as stream:
            stream.write(json.dumps(dict(task_id=1,solver='Other',problem='/problems/NEW.p',command='./other'))+'\n')
        metadata=json.loads((self.job/'metadata.json').read_text());metadata['task_count']=2
        (self.job/'metadata.json').write_text(json.dumps(metadata))
        full=self.root/'full';self.run_engine('default',full)
        self.assertEqual(len(self.summary_rows(full,'default')),2)
        self.run_engine('default',self.root/'empty',incremental=True)
        self.assertEqual(self.summary_rows(self.root/'empty','default'),[])
        results=self.job/'results'
        archive='batch_1_task_000000001_bbb_1_2.tar.gz'
        with tarfile.open(results/archive,'w:gz') as target:
            data=b'new result\n';info=tarfile.TarInfo('task_000000001_bbb.solver.log');info.size=len(data);target.addfile(info,io.BytesIO(data))
        with (results/'batch_000001_task_000000001.csv').open('w') as stream:
            writer=csv.DictWriter(stream,fieldnames=RESULT_COLUMNS);writer.writeheader();writer.writerow(dict(task_id=1,complete='true',status='ok',archive=archive))
        delta=self.root/'delta';self.run_engine('default',delta,incremental=True)
        self.assertEqual([row['task_id'] for row in self.summary_rows(delta,'default')],['1'])
        with tarfile.open(next(delta.glob('*_solver-stdout.tgz'))) as archive:
            names=[m.name for m in archive if m.isfile()]
        self.assertEqual(len(names),1);self.assertIn('NEW.p___000000001.log',names[0])
        # Default downloads must not consume the independent StarExec baseline.
        star=self.root/'star';self.run_engine('starexec',star,incremental=True)
        self.assertEqual(len(self.summary_rows(star,'starexec')),2)

    def test_failed_export_does_not_consume_changed_result(self):
        self.run_engine('default',self.root/'first',incremental=True)
        original=next((self.job/'results').glob('*.tar.gz'))
        revised=original.with_name('batch_0_task_000000000_abcdef_12345_1795517999.tar.gz')
        revised.write_bytes(b'corrupt archive')
        result_path=self.job/'results/batch_000000_task_000000000.csv'
        result_path.write_text(result_path.read_text().replace(original.name,revised.name))
        argv=[sys.executable,str(ROOT/'implementation/slurmy-sync.py'),self.identifier,'--host','fixture','--incremental','--output',str(self.root/'failed')]
        failed=subprocess.run(argv,cwd=self.root,env={**os.environ,**self.environment},capture_output=True,text=True)
        self.assertNotEqual(failed.returncode,0)
        with tarfile.open(revised,'w:gz') as archive:
            data=b'revised output\n';member=tarfile.TarInfo('task_000000000_abcdef.solver.log');member.size=len(data);archive.addfile(member,io.BytesIO(data))
        retry=self.root/'retry';self.run_engine('default',retry,incremental=True)
        self.assertEqual(len(self.summary_rows(retry,'default')),1)

    def test_debug_incremental_is_rejected_before_connecting(self):
        result=subprocess.run([sys.executable,str(ROOT/'implementation/slurmy-sync.py'),self.identifier,'--debug','--incremental','--host','missing'],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('cannot be combined',result.stderr)

    @unittest.skipUnless(importlib.util.find_spec('rich'),'Rich is optional')
    def test_interactive_cli_selects_from_real_lists_and_downloads(self):
        output=self.root/'interactive'
        result=subprocess.run([sys.executable,str(ROOT/'web-app/download.py'),'--interactive','--host','fixture','--output',str(output)],
                              env={**os.environ,**self.environment},input='3\n1\n',capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+'\n'+result.stderr)
        self.assertIn('Download Mode',result.stdout);self.assertIn('Archive demo (1)',result.stdout)
        self.assertEqual(len(list(output.glob('*.zip'))),2)

    def test_latest_cli_uses_the_same_dispatch_selection(self):
        output=self.root/'latest'
        result=subprocess.run([sys.executable,str(ROOT/'web-app/download.py'),'--latest','--host','fixture','--output',str(output)],
                              env={**os.environ,**self.environment},capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+'\n'+result.stderr)
        self.assertEqual(len(list(output.glob('*.tgz'))),3)
