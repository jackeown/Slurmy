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
