import _bootstrap  # noqa: F401 -- support direct unittest discovery
from types import SimpleNamespace
"""REST cluster projection and per-host connection settings in the interface."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import slurm_api
from slurmy_web import cluster_api,workflows
from slurmy_web.app import create_app


class TransportTests(unittest.TestCase):
    def test_rest_cluster_projection_preserves_core_socket_placement_and_partition_filter(self):
        nodes=[dict(name='n1',partitions=['CPU-amd'],threads=2,cores=4,sockets=2,cpus=16,alloc_cpus=8,alloc_idle_cpus=8,real_memory=1000,state=['MIXED']),
               dict(name='n2',partitions=['GPU'],threads=1,cores=4,sockets=1,cpus=4,alloc_cpus=4,alloc_idle_cpus=0,real_memory=1000,state=['ALLOCATED'])]
        jobs=[dict(job_id=100,array_job_id=dict(set=True,number=100),array_task_id=dict(set=True,number=0),user_name='alice',
             name='mine',partition='CPU-amd',job_state=['RUNNING'],nodes='n1',node_count=dict(set=True,number=1),cpus=dict(set=True,number=8),
             start_time=dict(set=True,number=1),time_limit=dict(set=True,number=10),state_reason='None',current_working_directory='/home/alice/slurmy/jobs/mine',
             comment='slurmy:v1:calls=10:batches=1:cores=2:isolation=socket:batch=0',
             job_resources=dict(allocated_nodes=[dict(nodename='n1',sockets={'1':{'cores':{str(i):'allocated' for i in range(4)}}})]))]
        def read(host,endpoint):return {'nodes':nodes,'partitions':[dict(name='CPU-amd'),dict(name='GPU')]} if endpoint!='jobs' else {'jobs':jobs}
        with patch.object(slurm_api,'read',side_effect=read),patch.object(slurm_api,'active_jobs',return_value=jobs),patch.object(slurm_api,'credentials',return_value=dict(user='alice')),patch.object(cluster_api.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='METADATA\n')):
            value=cluster_api.fetch_status('host','CPU-amd')
        self.assertEqual(value['summary']['total_cores'],8)
        self.assertEqual(value['users'][0]['allocated_cores'],4)
        self.assertEqual(value['jobs'][0]['node_placement']['n1']['socket_ids'],[1])
        self.assertEqual(value['jobs'][0]['solver_cores'],2)
        self.assertEqual(value['transport'],'REST API')

    def test_connection_settings_are_csrf_protected_and_tokens_never_reach_browser(self):
        with tempfile.TemporaryDirectory() as folder,patch.object(slurm_api,'CONFIG',Path(folder)/'settings.json'),patch.object(workflows,'migrate_user_runs'):
            app=create_app();client=app.test_client()
            with client.session_transaction() as session:session['csrf']='test'
            headers={'X-CSRF-Token':'test'}
            self.assertEqual(client.post('/api/connection',json={}).status_code,403)
            response=client.post('/api/connection?host=datalab',json=dict(mode='rest',url=slurm_api.DEFAULT_URL,version='v0.0.40'),headers=headers)
            self.assertEqual(response.status_code,200,response.json)
            self.assertNotIn('token',response.json)
            self.assertEqual(client.get('/api/connection?host=other').json['mode'],'ssh')
            html=client.get('/new').text
            self.assertIn('id="connection-mode"',html);self.assertIn('id="connection-url"',html)
            with patch.object(slurm_api,'request',return_value={'jobs':[]}),patch.object(slurm_api,'credentials',return_value=dict(user='alice',token='secret')):
                response=client.post('/api/connection/test?host=datalab',json={},headers=headers)
            self.assertEqual(response.status_code,200)
            self.assertNotIn('secret',response.text)
            slurm_api._credentials.clear();slurm_api._reads.clear()
