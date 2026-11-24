"""Run the real remote submitter/coordinator against a deterministic fake scheduler."""
import base64
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import test_batching
from slurmy import REPO


class ArraySubmissionTests(unittest.TestCase):
    def setup_job(self,count=6,overrides=None):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);source=self.root/'source';source.mkdir()
        assets=test_batching.SequentialBatchTests().prepare(source,[(False,False)]*count,1,overrides=overrides)
        self.home=self.root/'home';self.job=self.home/'slurmy/jobs/array-test'
        shutil.copytree(assets,self.job);(self.job/'.inputs-ready').touch();(self.job/'results').mkdir()
        self.bin=self.root/'bin';self.bin.mkdir()
        program='#!'+sys.executable+'\n'+'''import json,os,sys
from pathlib import Path
name=Path(sys.argv[0]).name
if name=='scontrol':
 if sys.argv[2]=='config':
  print('SelectTypeParameters = CR_CORE_MEMORY\\nTaskPlugin = task/cgroup\\nMaxArraySize = '+os.environ.get('TEST_ARRAY_SIZE','1001'))
 else:print('OverSubscribe=NO MaxTime=06:00:00')
elif name=='sinfo':print('2 8 2 64000')
elif name=='sacctmgr':print('tester|cpuopt||100|')
elif name=='squeue':pass # Earlier arrays complete before the next queue check.
elif name=='sbatch':
 path=Path(os.environ['HOME'])/'requests.json'
 rows=json.loads(path.read_text()) if path.exists() else []
 if os.environ.get('TEST_FAIL_AT')==str(len(rows)):
  print('test submission failure',file=sys.stderr);sys.exit(1)
 rows.append(sys.argv[1:]);path.write_text(json.dumps(rows))
 if os.environ.get('TEST_STOP_AFTER')==str(len(rows)):Path('.stopped').touch()
 print(1000+len(rows))
'''
        for name in ['scontrol','sinfo','sacctmgr','squeue','sbatch']:
            path=self.bin/name;path.write_text(program);path.chmod(0o755)
        self.env={**os.environ,'HOME':str(self.home),'USER':'tester','PATH':str(self.bin)+':'+os.environ['PATH']}

    def submit(self,window=100,resume=0,**env):
        return subprocess.run(['bash',str(REPO/'implementation/templates/remote_submit.sh'),
            'array-test','CPU-amd',str(window),'array-test',base64.b64encode(b'Array test').decode(),str(resume),'-','0','cpuopt'],
            env={**self.env,**env},capture_output=True,text=True,timeout=30)

    def requests(self):return json.loads((self.home/'requests.json').read_text())
    def ledger(self):
        with (self.job/'submission.csv').open(newline='') as stream:return list(csv.DictReader(stream))
    def assert_success(self,result):self.assertEqual(result.returncode,0,result.stdout+'\n'+result.stderr)

    def test_compatible_batches_share_one_array_and_keep_exact_batch_ids(self):
        self.setup_job();self.assert_success(self.submit())
        self.assertEqual(len(self.requests()),1);options=self.requests()[0]
        self.assertIn('--array=0-5',options);self.assertIn('--export=ALL,SLURMY_BATCH_OFFSET=0',options)
        self.assertIn('--comment=slurmy:v2:calls=6:batches=6:cores=2:isolation=shared:batch_offset=0',options)
        rows=self.ledger();self.assertEqual([int(r['batch_id']) for r in rows],list(range(6)))
        self.assertEqual([int(r['array_task_id']) for r in rows],list(range(6)))
        self.assertEqual({r['slurm_job_id'] for r in rows},{'1001'})
        self.assertEqual((self.job/'submission.state').read_text(),'complete 6 6\n')
        self.assertEqual(json.loads((self.job/'metadata.json').read_text())['planned_batch_tasks'],[[i] for i in range(6)])

    def test_memory_or_wall_request_changes_split_arrays(self):
        for overrides in [[{'mem_limit':value} for value in ['100MiB']*2+['200MiB']*2],
                          [{'wc_limit':value} for value in [6,6,150,150]]]:
            with self.subTest(overrides=overrides):
                self.setup_job(4,overrides);self.assert_success(self.submit())
                requests=self.requests();self.assertEqual(len(requests),2)
                self.assertTrue(all('--array=0-1' in options for options in requests))
                self.assertIn('--export=ALL,SLURMY_BATCH_OFFSET=2',requests[1])

    def test_cluster_array_size_and_window_caps_preserve_all_batches(self):
        for window,limit,expected in [(100,'3',['0-2','0-2','0-1']),(2,'1001',['0-1']*4)]:
            with self.subTest(window=window,limit=limit):
                self.setup_job(8);self.assert_success(self.submit(window,TEST_ARRAY_SIZE=limit))
                actual=[next(o[8:] for o in request if o.startswith('--array=')) for request in self.requests()]
                self.assertEqual(actual,expected)
                self.assertEqual([int(r['batch_id']) for r in self.ledger()],list(range(8)))
                for row in self.ledger():self.assertEqual(int(row['offset'])+int(row['array_task_id']),int(row['batch_id']))

    def test_resume_legacy_ledger_retries_only_unfinished_contiguous_groups(self):
        self.setup_job()
        with (self.job/'submission.csv').open('w',newline='') as stream:
            writer=csv.writer(stream);writer.writerow(['slurm_job_id','offset','array_size','submitted_epoch','batch_id'])
            writer.writerows([[100+i,i,1,1,i] for i in range(6)])
        for task in [0,2,3]:(self.job/'results'/f'batch_{task:06d}_task_{task:09d}.csv').write_text(f'task_id,complete\n{task},true\n')
        self.assert_success(self.submit(resume=1))
        self.assertEqual(len(self.requests()),2)
        new=self.ledger()[6:];self.assertEqual([int(r['batch_id']) for r in new],[1,4,5])
        self.assertEqual([int(r['array_task_id']) for r in new],[0,0,1])
        self.assertTrue(all(r['array_task_id']=='' for r in self.ledger()[:6]))
        self.assertEqual((self.job/'.resume-pending').read_text(),'')
        self.assertEqual((self.job/'submission.state').read_text(),'complete 6 6\n')

    def test_failure_after_one_array_continues_without_duplicate_batches(self):
        self.setup_job()
        failed=self.submit(TEST_ARRAY_SIZE='3',TEST_FAIL_AT='1');self.assertNotEqual(failed.returncode,0)
        self.assertEqual(len(self.ledger()),3)
        self.assert_success(self.submit(TEST_ARRAY_SIZE='3'))
        self.assertEqual([int(r['batch_id']) for r in self.ledger()],list(range(6)))
        self.assertEqual(len(self.requests()),2)

    def test_legacy_coordinator_forces_single_element_arrays(self):
        self.setup_job(4)
        (self.job/'submit_coordinator.py').write_text("""import json,os,sys
from pathlib import Path
if sys.argv[1]=='attempt':
 path=Path(os.environ['HOME'])/'requests.json'
 rows=json.loads(path.read_text()) if path.exists() else []
 rows.append(sys.argv[5:]);path.write_text(json.dumps(rows))
 print('ACCEPTED|'+str(1000+len(rows))+'|legacy capacity')
""")
        self.assert_success(self.submit())
        self.assertEqual(len(self.requests()),4)
        self.assertTrue(all('--array=0-0' in options for options in self.requests()))
        self.assertEqual([int(row['batch_id']) for row in self.ledger()],list(range(4)))

    def test_legacy_worker_keeps_explicit_batch_mapping(self):
        self.setup_job(4)
        (self.job/'batch.sh').write_text('BATCH_ID=${SLURMY_BATCH_ID:-${SLURM_ARRAY_TASK_ID:?}}\n')
        self.assert_success(self.submit())
        self.assertEqual(len(self.requests()),4)
        for batch,options in enumerate(self.requests()):
            self.assertIn('--array=0-0',options)
            self.assertIn('--export=ALL,SLURMY_BATCH_ID='+str(batch),options)

    def test_stop_marker_blocks_further_arrays_after_recording_acceptance(self):
        self.setup_job()
        stopped=self.submit(TEST_ARRAY_SIZE='3',TEST_STOP_AFTER='1')
        self.assertNotEqual(stopped.returncode,0);self.assertEqual(len(self.requests()),1)
        self.assertEqual(len(self.ledger()),3);self.assertTrue((self.job/'.stopped').is_file())
