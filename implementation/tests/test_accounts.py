"""Account selection reaches SLURM and never inherits another account's limit."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
import submit_coordinator as coordinator
import local_dispatch
import slurm_api


class AccountTests(unittest.TestCase):
    def test_association_limits_ignore_other_accounts_partitions_and_trailing_delimiters(self):
        rows='tester|default||50|\ntester|cpuopt|||\ntester|cpuopt|other|5|\n'
        self.assertIsNone(coordinator.association_limit(rows,'cpuopt','CPU-amd','tester'))
        self.assertEqual(coordinator.association_limit(rows,'default','CPU-amd','tester'),50)
        self.assertEqual(coordinator.association_limit(rows+'tester|cpuopt|CPU-amd|120|\n','cpuopt','CPU-amd','tester'),120)

    def coordinate(self,account,count,rejection=None):
        with tempfile.TemporaryDirectory() as folder:
            queue=''.join(f'{i}|{folder}/slurmy/jobs/test-job|\n' for i in range(count))
            def command(argv,**kwargs):
                if argv[0]=='squeue':
                    self.assertEqual(argv[argv.index('-A')+1],account)
                    return subprocess.CompletedProcess(argv,0,queue,'')
                self.assertEqual(argv[0],'sbatch');self.assertIn('--account='+account,argv)
                return subprocess.CompletedProcess(argv,1 if rejection else 0,'' if rejection else '12345\n',rejection or '')
            argv=['submit_coordinator.py','attempt','test-job','100','--','--account='+account,'--partition=CPU-amd','batch.sh']
            with patch.object(Path,'home',return_value=Path(folder)),patch.object(sys,'argv',argv),patch.dict(os.environ,USER='tester'),patch.object(coordinator,'reported_limit',return_value=None),patch.object(coordinator,'command',side_effect=command) as calls,patch('builtins.print') as output:
                code=coordinator.main()
            return code,output.call_args.args[0],calls.call_count

    def test_custom_account_can_submit_above_fifty_but_default_still_waits(self):
        self.assertTrue(self.coordinate('cpuopt',60)[1].startswith('ACCEPTED|'))
        code,message,calls=self.coordinate('default',50)
        self.assertTrue(message.startswith('WAIT|'));self.assertEqual(calls,1)

    def test_unauthorized_account_is_an_error_without_silent_default_fallback(self):
        code,message,calls=self.coordinate('unauthorized',0,'Invalid account or account/partition combination specified')
        self.assertEqual(code,2);self.assertIn('Invalid account',message);self.assertEqual(calls,2)

    def test_rest_queue_filters_the_selected_account_and_payload_preserves_it(self):
        jobs=[dict(job_id=1,user_name='tester',account='default'),dict(job_id=2,user_name='tester',account='cpuopt')]
        with patch.object(slurm_api,'credentials',return_value={'user':'tester'}),patch.object(slurm_api,'submission_queue',return_value=jobs):
            result=local_dispatch.rpc('host',dict(argv=['squeue','-A','cpuopt','-o','%i|%Z|%k']))
        self.assertTrue(result['stdout'].startswith('2|'));self.assertNotIn('1|',result['stdout'])
        payload=slurm_api.batch_request(['--account=cpuopt'],'#!/bin/bash\ntrue','/tmp',{})
        self.assertEqual(payload['job']['account'],'cpuopt')

    def test_policy_lookup_is_cached_without_inventing_a_limit(self):
        slurm_api._account_limits.clear()
        with patch.object(slurm_api.subprocess,'run',return_value=subprocess.CompletedProcess([],0,'tester|cpuopt|||\n','')) as run:
            self.assertIsNone(slurm_api.account_limit('test-account-host','cpuopt','CPU-amd'))
            self.assertIsNone(slurm_api.account_limit('test-account-host','cpuopt','CPU-amd'))
        self.assertEqual(run.call_count,1)

    def test_rest_wait_does_not_stop_an_unlimited_account_at_fifty(self):
        def process(code):
            result=Mock(stdin=io.StringIO(),stdout=io.StringIO(''))
            result.wait.return_value=code;result.poll.return_value=code
            return result
        jobs=[dict(job_id=i,user_name='tester',account='cpuopt',current_working_directory='/home/tester/slurmy/jobs/job') for i in range(60)]
        args=['job','CPU-amd','100','job','am9i','0','-','0','cpuopt']
        with patch.object(slurm_api,'enabled',return_value=True),patch.object(slurm_api,'credentials',return_value={'user':'tester','home':'/home/tester'}),patch.object(slurm_api,'submission_queue',return_value=jobs) as queue,patch.object(slurm_api,'account_limit',return_value=None),patch.object(local_dispatch.subprocess,'Popen',side_effect=[process(75),process(0)]),patch.object(local_dispatch.time,'sleep'),patch('builtins.print'):
            self.assertEqual(local_dispatch._run('host',args,script='true'),0)
        self.assertEqual(queue.call_count,1)

    def test_resume_returns_the_saved_account_to_the_local_driver(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            (job/'.stopped').touch();(job/'remote_submit.sh').touch()
            (job/'submission_context.sh').write_text('SAVED_PARTITION=CPU-amd\nSAVED_MAX_OUTSTANDING=100\nSAVED_JOB_PREFIX=job\nSAVED_JOB_NAME_B64=am9i\nSAVED_RESERVATION=\nSAVED_ACCOUNT=cpuopt\n')
            tools=root/'tools';tools.mkdir();(tools/'squeue').write_text('#!/bin/sh\nexit 0\n');(tools/'squeue').chmod(0o755)
            result=subprocess.run(['bash',str(Path(__file__).parent.parent/'templates/remote_resume.sh'),'job'],
                                  env={**os.environ,'HOME':folder,'USER':'tester','PATH':str(tools)+os.pathsep+os.environ['PATH']},capture_output=True,text=True,check=True)
            args=json.loads(result.stdout.split('SLURMY_RESUME_ARGS ',1)[1])
            self.assertEqual(len(args),9);self.assertEqual(args[8],'cpuopt')


if __name__=='__main__':unittest.main()
