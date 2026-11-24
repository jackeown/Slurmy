"""REST authentication, request semantics, and generated resource options."""
import io
import json
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import urllib.error

import slurm_api as api


class ApiTests(unittest.TestCase):
    def setUp(self):
        folder=tempfile.TemporaryDirectory();self.addCleanup(folder.cleanup)
        self.path=Path(folder.name)/'settings.json'
        p=patch.object(api,'CONFIG',self.path);p.start();self.addCleanup(p.stop)
        api._credentials.clear();api._reads.clear();api._submission_metadata.clear()
        self.addCleanup(api._credentials.clear);self.addCleanup(api._reads.clear);self.addCleanup(api._submission_metadata.clear)
        self.auth=dict(token='private-jwt',user='alice',home='/home/alice',expires=time.time()+3600)

    def response(self,value):return io.BytesIO(json.dumps(value).encode())

    def test_settings_are_per_host_private_and_never_contain_tokens(self):
        self.assertFalse(api.enabled('datalab'))
        api.save_settings('datalab',dict(mode='rest',url=api.DEFAULT_URL,version='v0.0.40'))
        self.assertTrue(api.enabled('datalab'));self.assertFalse(api.enabled('friend'))
        self.assertEqual(self.path.stat().st_mode & 0o777,0o600)
        self.assertNotIn('token',self.path.read_text())
        for url in ['file:///etc/passwd','http://user:password@host:6820','http://host:bad','http://host/path','http://host\n/']:
            with self.assertRaises(ValueError):api.save_settings('datalab',dict(mode='rest',url=url))

    def test_tokens_bootstrap_once_and_renew_after_authentication_rejection(self):
        bootstrap=SimpleNamespace(returncode=0,stdout=json.dumps(self.auth))
        with patch.object(api.subprocess,'run',return_value=bootstrap) as ssh, patch.object(api.urllib.request,'build_opener') as opener:
            opener.return_value.open.side_effect=[self.response({'jobs':[]}),self.response({'nodes':[]}),
                urllib.error.HTTPError(api.DEFAULT_URL,401,'expired',{},io.BytesIO(b'')),self.response({'jobs':[]})]
            api.request('datalab','jobs');api.request('datalab','nodes');api.request('datalab','jobs')
            self.assertEqual(ssh.call_count,2)
            request=opener.return_value.open.call_args.args[0]
            self.assertEqual(request.get_header('X-slurm-user-token'),'private-jwt')
            self.assertNotIn('private-jwt',request.full_url)
            self.assertFalse(self.path.exists())

    def test_post_timeout_is_not_replayed_or_silently_fallen_back_to_ssh(self):
        with patch.object(api,'credentials',return_value=self.auth),patch.object(api.urllib.request,'build_opener') as opener:
            opener.return_value.open.side_effect=TimeoutError()
            with self.assertRaisesRegex(RuntimeError,'No SSH fallback'):api.request('datalab','job/submit','POST',{})
            self.assertEqual(opener.return_value.open.call_count,1)
        with self.assertRaisesRegex(RuntimeError,'redirects'):
            api.NoRedirect().redirect_request(None,None,302,'',{},'http://other')

    def test_generated_sbatch_options_keep_limits_placement_signals_and_environment(self):
        options=['--parsable','--partition=CPU-amd','--job-name=test','--comment=batch=3','--requeue',
            '--open-mode=append','--nodes=1','--ntasks=1','--cpus-per-task=32','--threads-per-core=1',
            '--distribution=block:block','--mem=640M','--time=9','--array=0-0',
            '--export=ALL,SLURMY_BATCH_ID=3','--output=logs/slurm_%A_%a.out','--signal=B:TERM@30',
            '--reservation=test-reservation','--ntasks-per-socket=1','--cores-per-socket=32',
            '--sockets-per-node=1','--exclusive','batch.sh']
        payload=api.batch_request(options,'#!/bin/bash\ntrue\n','/home/alice/job',{'PATH':'/bin','SLURM_JWT':'secret'})
        job=payload['job']
        self.assertEqual(job['cpus_per_task'],32);self.assertEqual(job['memory_per_node']['number'],640)
        self.assertEqual(job['time_limit']['number'],9);self.assertEqual(job['exclusive'],['true'])
        self.assertEqual(job['kill_warning_flags'],['BATCH_JOB']);self.assertEqual(job['reservation'],'test-reservation')
        self.assertIn('SLURMY_BATCH_ID=3',job['environment']);self.assertIn('SLURM_SUBMIT_DIR=/home/alice/job',job['environment'])
        self.assertFalse(any(item.startswith('SLURM_JWT=') for item in job['environment']))
        with self.assertRaisesRegex(ValueError,'Unsupported sbatch'):
            api.batch_request(['--unknown=1'],'true','/tmp',{})

    def test_zero_memory_and_unlimited_time_have_distinct_rest_representations(self):
        job=api.batch_request(['--mem=0M','--time=0'],'true','/tmp',{})['job']
        self.assertEqual(job['memory_per_node'],dict(set=True,number=0))
        self.assertEqual(job['time_limit'],dict(infinite=True))

    def test_build_time_formats_and_single_task_distribution(self):
        self.assertEqual(api.time_minutes('00:05:00'),5)
        self.assertEqual(api.time_minutes('2-03:04:05'),3065)
        payload=api.batch_request(['--distribution=block:block','--nodes=1','--ntasks=1','--time=00:05:00','--error=build.log'], 'true','/tmp',{})
        self.assertNotIn('distribution',payload['job'])
        self.assertEqual(payload['job']['time_limit']['number'],5)
        self.assertEqual(payload['job']['standard_error'],'build.log')

    def test_cancellation_checks_per_job_error_even_when_http_succeeds(self):
        with patch.object(api,'credentials',return_value=self.auth),patch.object(api.urllib.request,'build_opener') as opener:
            opener.return_value.open.return_value=self.response(dict(errors=[],status=[dict(error=dict(code=1,message='permission denied'))]))
            with self.assertRaisesRegex(RuntimeError,'permission denied'):api.request('host','job/1','DELETE')

    def test_slow_read_does_not_block_a_different_endpoint(self):
        import threading
        barrier=threading.Barrier(2);errors=[]
        def request(host,endpoint):
            barrier.wait(timeout=2)
            return {endpoint:[]}
        def run(endpoint):
            try:api.read('host',endpoint,max_age=0)
            except Exception as error:errors.append(error)
        with patch.object(api,'request',side_effect=request):
            threads=[threading.Thread(target=run,args=(endpoint,)) for endpoint in ('nodes','jobs')]
            for thread in threads:thread.start()
            for thread in threads:thread.join(timeout=3)
        self.assertFalse(errors,errors)

    def test_failed_refresh_does_not_return_an_expired_cached_response(self):
        with patch.object(api,'request',side_effect=[{'jobs':[]},RuntimeError('offline')]) as request:
            first=api.read('host','jobs')
            self.assertIs(api.read('host','jobs'),first)
            with self.assertRaisesRegex(RuntimeError,'offline'):api.read('host','jobs',max_age=0)
            self.assertEqual(request.call_count,2)

    def test_compact_state_polling_resolves_new_jobs_and_pending_array_elements(self):
        master=dict(job_id=10, array_job_id=dict(set=True, number=10), array_task_id=dict(set=False,number=0),
                    array_task_string='1-3', user_name='alice', current_working_directory='/home/alice/job')
        other=dict(job_id=20,user_name='alice',current_working_directory='/home/alice/other')
        states=[dict(job_id='10_1',state=['PENDING']),dict(job_id='10_2',state=['PENDING']),dict(job_id='10_3',state=['PENDING'])]
        def response(host,endpoint):
            if endpoint=='jobs/state':return dict(jobs=states)
            if endpoint=='jobs':return dict(jobs=[master])
            if endpoint=='job/20':return dict(jobs=[other])
            raise AssertionError(endpoint)
        with patch.object(api,'request',side_effect=response) as request:
            first=api.submission_queue('host')
            self.assertEqual([api.job_id(j) for j in first],['10_1','10_2','10_3'])
            self.assertEqual(len(api.array_elements(master)),3)
            states.pop();states.append(dict(job_id='20',state=['RUNNING']))
            second=api.submission_queue('host')
            self.assertEqual([api.job_id(j) for j in second],['10_1','10_2','20'])
            self.assertEqual(sum(call.args[1]=='jobs' for call in request.call_args_list),1)
            self.assertEqual(sum(call.args[1]=='job/20' for call in request.call_args_list),1)


    def test_array_payload_preserves_local_indices_offset_account_and_arbitrary_comment(self):
        for specification in ('0-0', '0-5', '2-5', '2,3,5', '0-8:2%3'):
            with self.subTest(specification=specification):
                payload = api.batch_request([
                    '--array='+specification, '--account=research',
                    '--comment=slurmy:v2 arbitrary public comment',
                    '--export=ALL,SLURMY_BATCH_OFFSET=120'], 'true', '/work/job', {})
                self.assertEqual(payload['job']['array'], specification)
                self.assertEqual(payload['job']['account'], 'research')
                self.assertEqual(payload['job']['comment'], 'slurmy:v2 arbitrary public comment')
                self.assertIn('SLURMY_BATCH_OFFSET=120', payload['job']['environment'])

    def test_remembered_arrays_resolve_compact_states_without_parent_or_extra_reads(self):
        for specification, indices in [('0-0', [0]), ('0-3', [0,1,2,3]),
                                       ('2-5', [2,3,4,5]), ('2,3,5', [2,3,5]),
                                       ('0-6:2%2', [0,2,4,6])]:
            with self.subTest(specification=specification):
                api._submission_metadata.clear()
                with patch.object(api, 'request', return_value={'jobs': []}):
                    api.submission_queue('host')
                payload = api.batch_request(['--array='+specification, '--account=research',
                                            '--comment=arbitrary v2 string'], 'true', '/work/job', {})
                with patch.object(api, 'credentials', return_value=self.auth):
                    api.remember_submission('host', 100, payload)
                # Parent placeholder and duplicate element must not consume extra slots.
                states = [dict(job_id=100, state=['PENDING'])] + [
                    dict(job_id=f'100_{index}', state=['RUNNING' if index == indices[0] else 'PENDING'])
                    for index in indices]
                states.append(states[-1])
                with patch.object(api, 'request', return_value={'jobs': states}) as request:
                    queue = api.submission_queue('host')
                request.assert_called_once_with('host', 'jobs/state')
                self.assertEqual([api.job_id(j) for j in queue], [f'100_{i}' for i in indices])
                self.assertTrue(all(j['account'] == 'research' and j['comment'] == 'arbitrary v2 string'
                                    and j['current_working_directory'] == '/work/job' for j in queue))

    def test_numeric_materialized_ids_and_grouped_states_use_element_identity(self):
        master = dict(job_id=100, array_job_id=100, array_task_id={'set':False},
                      array_task_string='2-5', account='research', user_name='alice',
                      current_working_directory='/work/job')
        element = dict(master, job_id=201, array_task_id=2, array_task_string='')
        states = [dict(job_id=201, array_job_id=100, array_task_id=2, state=['RUNNING']),
                  dict(job_id=100, array_job_id=100, array_task_id={'set':False},
                       array_task_string='3-5', state=['PENDING'])]
        with patch.object(api, 'request', side_effect=[{'jobs':states}, {'jobs':[master, element]}]):
            queue = api.submission_queue('host')
        self.assertEqual([api.job_id(j) for j in queue], ['100_2','100_3','100_4','100_5'])
        # Some compact responses supply only the materialized numeric job ID.
        with patch.object(api, 'request', return_value={'jobs':[dict(job_id=201, state=['RUNNING'])]}):
            self.assertEqual([api.job_id(j) for j in api.submission_queue('host')], ['100_2'])

    def test_explicit_terminal_element_overrides_pending_descriptor_in_either_order(self):
        master = dict(job_id=100, array_job_id=100, array_task_string='2-5',
                      user_name='alice', job_state=['PENDING'])
        done = dict(master, job_id=201, array_task_id=3, job_state=['COMPLETED'])
        for rows in ([done, master], [master, done]):
            with patch.object(api, 'read', return_value={'jobs':rows}):
                self.assertEqual([api.job_id(j) for j in api.active_jobs('host')],
                                 ['100_2','100_4','100_5'])
            api._submission_metadata.clear()
            states = [dict(j, state=j['job_state']) for j in rows]
            with patch.object(api, 'request', side_effect=[{'jobs':states}, {'jobs':[master]}]):
                self.assertEqual(set(api.job_id(j) for j in api.submission_queue('host')),
                                 {'100_2','100_4','100_5'})

    def test_array_expansion_deduplicates_indices_and_rejects_invalid_ranges(self):
        master = dict(job_id=100, array_job_id=100, array_task_string='2,2-6:2%3')
        self.assertEqual([api.job_id(j) for j in api.array_elements(master)], ['100_2','100_4','100_6'])
        for specification in ('4-2', '0-3:0', 'garbage'):
            with self.assertRaises(RuntimeError):
                api.array_elements(dict(master, array_task_string=specification))

    def test_remembered_array_resolves_new_numeric_materialized_id_and_caches_it(self):
        with patch.object(api, 'request', return_value={'jobs': []}):
            api.submission_queue('host')
        payload = api.batch_request(['--array=2-5','--account=research'], 'true', '/work/job', {})
        with patch.object(api, 'credentials', return_value=self.auth):
            api.remember_submission('host', 100, payload)
        # Unlike parent_index compact IDs, a new numeric materialized ID cannot
        # be inferred from the remembered parent. Resolve it once over REST.
        states = [dict(job_id='201', state=['RUNNING']),
                  dict(job_id='100_2', state=['PENDING'])]
        materialized = dict(job_id=201, array_job_id=100, array_task_id=3,
                            user_name='alice', account='research', current_working_directory='/work/job')
        with patch.object(api, 'request', side_effect=[{'jobs':states}, {'jobs':[materialized]}]) as request:
            queue = api.submission_queue('host')
        self.assertEqual([call.args[1] for call in request.call_args_list], ['jobs/state','job/201'])
        self.assertEqual([api.job_id(j) for j in queue], ['100_3','100_2'])
        self.assertTrue(all(j['account'] == 'research' for j in queue))
        with patch.object(api, 'request', return_value={'jobs':states}) as request:
            self.assertEqual([api.job_id(j) for j in api.submission_queue('host')], ['100_3','100_2'])
        request.assert_called_once_with('host', 'jobs/state')

    def test_terminal_wildcard_is_ignored_but_unknown_active_identifiers_fail_closed(self):
        master = dict(job_id=100, array_job_id=100, array_task_string='0-3',
                      user_name='alice', account='research')
        states = [dict(job_id='1136022_*', state=['CANCELLED']),
                  dict(job_id='100_0', state=['RUNNING']),
                  dict(job_id='100_1', state=['PENDING']),
                  dict(job_id='100_2', state=['CANCELLED']),
                  dict(job_id='100_3', state=['COMPLETED'])]
        with patch.object(api, 'request', side_effect=[{'jobs':states}, {'jobs':[master]}]):
            self.assertEqual([api.job_id(j) for j in api.submission_queue('host')], ['100_0','100_1'])
        for identifier in ('1136022_*', '100_[0-3]', 'unexpected'):
            for state in (['PENDING'], ['CANCELLED','COMPLETING']):
                with self.subTest(identifier=identifier, state=state):
                    with patch.object(api, 'request', return_value={'jobs':[dict(job_id=identifier, state=state)]}):
                        with self.assertRaisesRegex(RuntimeError, 'Unrecognized SLURM state identifier'):
                            api.submission_queue('host')
        with patch.object(api, 'request', return_value={'jobs':[states[0]]}) as request:
            self.assertEqual(api.submission_queue('host'), [])
        request.assert_called_once_with('host', 'jobs/state')

    def test_metadata_index_uses_snapshot_while_submission_is_remembered(self):
        import threading
        entered = threading.Event()
        resume = threading.Event()
        errors = []
        result = []
        class SlowID:
            def __str__(self):
                entered.set()
                if not resume.wait(3):
                    raise RuntimeError('metadata index synchronization timed out')
                return '9'
        config = api.settings('host')
        key = ('host', config['url'], config['version'])
        api._submission_metadata[key] = dict(time=time.monotonic(), jobs={
            '9':dict(job_id=SlowID()),
            '100':dict(job_id=100, array_job_id=100, array_task_string='0-3')})
        def poll():
            try:
                result.extend(api.submission_queue('host'))
            except Exception as error:
                errors.append(error)
        with patch.object(api, 'request', return_value={'jobs':[dict(job_id='100_0', state=['PENDING'])]}), \
             patch.object(api, 'credentials', return_value=self.auth):
            thread = threading.Thread(target=poll)
            thread.start()
            try:
                self.assertTrue(entered.wait(3))
                api.remember_submission('host', 200, {'job':dict(
                    current_working_directory='/work/job', account='research', array='0-3')})
            finally:
                resume.set()
                thread.join(3)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, errors)
        self.assertEqual([api.job_id(j) for j in result], ['100_0'])
        self.assertTrue(all(f'200_{i}' in api._submission_metadata[key]['jobs'] for i in range(4)))

    def test_refresh_keeps_submission_remembered_during_full_jobs_request(self):
        import threading
        entered = threading.Event()
        resume = threading.Event()
        errors = []
        result = []
        config = api.settings('host')
        key = ('host', config['url'], config['version'])
        api._submission_metadata[key] = dict(time=time.monotonic()-301, jobs={})
        def request(host, endpoint):
            if endpoint == 'jobs/state':
                return {'jobs':[dict(job_id='200_0', state=['PENDING'])]}
            if endpoint == 'jobs':
                entered.set()
                if not resume.wait(3):
                    raise RuntimeError('metadata refresh synchronization timed out')
                return {'jobs':[]}  # Snapshot predates the new submission.
            raise AssertionError(endpoint)
        def poll():
            try:
                result.extend(api.submission_queue('host'))
            except Exception as error:
                errors.append(error)
        with patch.object(api, 'request', side_effect=request), \
             patch.object(api, 'credentials', return_value=self.auth):
            thread = threading.Thread(target=poll)
            thread.start()
            try:
                self.assertTrue(entered.wait(3))
                api.remember_submission('host', 200, {'job':dict(
                    current_working_directory='/work/job', account='research', array='0-3')})
            finally:
                resume.set()
                thread.join(3)
        self.assertFalse(thread.is_alive())
        self.assertFalse(errors, errors)
        self.assertEqual([api.job_id(j) for j in result], ['200_0'])
        self.assertTrue(all(f'200_{i}' in api._submission_metadata[key]['jobs'] for i in range(4)))
