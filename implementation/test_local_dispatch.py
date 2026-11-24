"""The active launcher is finite and waits locally, without detached controllers."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import local_dispatch
import slurm_api

TEMPLATES=Path(__file__).parent/'templates'


class LocalDispatchTests(unittest.TestCase):
    def test_remote_wave_returns_capacity_wait_and_never_launches_detached_work(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/test';job.mkdir(parents=True)
            (job/'remote_submit.sh').write_text('test "$SLURMY_MANAGED_CONTROLLER" = 1\ntest "$SLURMY_CONTROLLER_DEADLINE" -gt "$(date +%s)"\nexit 75\n')
            result=subprocess.run(['bash',str(TEMPLATES/'remote_dispatch.sh'),'test'],env={**os.environ,'HOME':folder},capture_output=True,text=True,timeout=3)
            self.assertEqual(result.returncode,75,result.stderr)
            self.assertIn('Slurmy job ID: test',result.stdout)
            self.assertFalse((job/'.submission-controller.id').exists())
            self.assertNotIn('nohup',result.stdout)

    def test_local_lock_reports_activity_and_is_released_after_failure(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(slurm_api,'CONFIG',Path(folder)/'settings.json'):
            self.assertFalse(local_dispatch.is_active('host','job'))
            def driver(*args):
                self.assertTrue(local_dispatch.is_active('host','job'))
                raise RuntimeError('interrupted')
            with patch.object(local_dispatch,'_run',side_effect=driver):
                with self.assertRaisesRegex(RuntimeError,'interrupted'):local_dispatch.run('host',['job'])
            self.assertFalse(local_dispatch.is_active('host','job'))

    def test_rpc_uses_requested_queue_columns_and_rest_cancellation(self):
        row=dict(job_id=123,user_name='alice',current_working_directory='/home/alice/job',comment='fixture')
        with patch.object(slurm_api,'credentials',return_value=dict(user='alice')),patch.object(slurm_api,'submission_queue',return_value=[row]):
            self.assertEqual(local_dispatch.rpc('host',dict(argv=['squeue','-o','%Z']))['stdout'],'/home/alice/job\n')
            self.assertEqual(local_dispatch.rpc('host',dict(argv=['squeue','-o','%i|%Z']))['stdout'],'123|/home/alice/job\n')
        with patch.object(slurm_api,'request',return_value={}) as request:
            self.assertEqual(local_dispatch.rpc('host',dict(argv=['scancel','123_0']))['returncode'],0)
            request.assert_called_once_with('host','job/123_0','DELETE')

    def test_rpc_posts_exact_batch_request_and_returns_accepted_id(self):
        message=dict(argv=['sbatch','--partition=CPU-amd','--nodes=1','batch.sh'],script='#!/bin/bash\ntrue\n',cwd='/home/a/job',environment={'PATH':'/bin'})
        with patch.object(slurm_api,'request',return_value={'job_id':123,'errors':[]}) as request:
            response=local_dispatch.rpc('host',message)
        self.assertEqual(response['stdout'],'123\n')
        self.assertEqual(request.call_args.args[:3],('host','job/submit','POST'))
        self.assertEqual(request.call_args.args[3]['job']['minimum_nodes'],1)

    def test_full_rest_window_waits_locally_without_opening_more_ssh_sessions(self):
        import io
        class Process:
            def __init__(self,code):
                self.code=code;self.stdout=io.StringIO('wave output\n');self.stdin=io.StringIO()
            def wait(self,timeout=None):return self.code
            def poll(self):return self.code
        args=['job','CPU-amd','1','prefix','bmFtZQ==','0','-','0']
        auth=dict(user='alice',home='/home/alice')
        queue=[[dict(user_name='alice',current_working_directory='/home/alice/slurmy/jobs/job')],[]]
        with patch.object(slurm_api,'enabled',return_value=True),patch.object(slurm_api,'credentials',return_value=auth), \
             patch.object(slurm_api,'submission_queue',side_effect=queue) as status, \
             patch.object(local_dispatch.time,'sleep') as sleep, \
             patch.object(local_dispatch.subprocess,'Popen',side_effect=[Process(75),Process(0)]) as ssh:
            self.assertEqual(local_dispatch._run('host',args,script='true',poll_seconds=1),0)
        self.assertEqual(ssh.call_count,2)
        self.assertEqual(status.call_count,2)
        self.assertEqual(sleep.call_count,2)


    def test_new_array_bridge_counts_each_local_element_for_its_account_and_stops_individually(self):
        with tempfile.TemporaryDirectory() as folder, \
             patch.object(slurm_api, 'CONFIG', Path(folder)/'settings.json'), \
             patch.object(slurm_api, 'credentials', return_value=dict(user='alice')):
            slurm_api._submission_metadata.clear()
            self.addCleanup(slurm_api._submission_metadata.clear)
            with patch.object(slurm_api, 'request', return_value={'jobs': []}):
                slurm_api.submission_queue('host')
            message = dict(argv=['sbatch', '--array=2-5', '--account=research',
                                 '--comment=public v2 arbitrary string',
                                 '--export=ALL,SLURMY_BATCH_OFFSET=100', 'batch.sh'],
                           script='true', cwd='/home/alice/job', environment={})
            with patch.object(slurm_api, 'request', return_value={'job_id':123}) as request:
                self.assertEqual(local_dispatch.rpc('host', message)['stdout'], '123\n')
                payload = request.call_args.args[3]
                self.assertEqual(payload['job']['array'], '2-5')
                self.assertIn('SLURMY_BATCH_OFFSET=100', payload['job']['environment'])
            states = [dict(job_id=123, state=['PENDING'])] + [
                dict(job_id=f'123_{i}', state=['PENDING']) for i in range(2,6)]
            query = dict(argv=['squeue', '-A', 'research', '-o', '%i|%Z|%k'])
            with patch.object(slurm_api, 'request', return_value={'jobs':states}) as request:
                lines = local_dispatch.rpc('host', query)['stdout'].splitlines()
                self.assertEqual(lines, [f'123_{i}|/home/alice/job|public v2 arbitrary string'
                                         for i in range(2,6)])
                request.assert_called_once_with('host', 'jobs/state')
                other = local_dispatch.rpc('host', dict(argv=['squeue','-A','other','-o','%Z']))
                self.assertEqual(other['stdout'].strip(), '')
            with patch.object(slurm_api, 'request', return_value={}) as request:
                local_dispatch.rpc('host', dict(argv=['scancel', '123_3', '123_5']))
                self.assertEqual([call.args for call in request.call_args_list],
                                 [('host','job/123_3','DELETE'), ('host','job/123_5','DELETE')])
            states[2]['state'] = ['CANCELLED']  # local index 3 has stopped
            states[4]['state'] = ['CANCELLED']  # local index 5 has stopped
            with patch.object(slurm_api, 'request', return_value={'jobs':states}):
                lines = local_dispatch.rpc('host', query)['stdout'].splitlines()
                self.assertEqual([line.split('|')[0] for line in lines], ['123_2','123_4'])
