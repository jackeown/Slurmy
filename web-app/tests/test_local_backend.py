"""Exercise native preparation, execution, cancellation and downloads without SSH."""
import _bootstrap  # noqa: F401
import csv
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import local_backend as local
from slurmy_web import workflows
from slurmy_web.app import create_app
from slurmy_web.local_monitor import collect


@unittest.skipUnless(sys.platform in {'linux','darwin'},'Native backend requires Linux or macOS')
class LocalBackendTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.directory=Path(temporary.name);self.environment=patch.dict(os.environ,SLURMY_LOCAL_ROOT=str(self.directory/'state'));self.environment.start();self.addCleanup(self.environment.stop)
        self.runtime=self.directory/'runtime';self.runtime.mkdir()
        self.solver=self.runtime/'solver';self.solver.write_text('#!/bin/sh\necho "% SZS status Theorem"\necho "solver stderr" >&2\n');self.solver.chmod(0o755)
        # A small limiter fixture leaves timing to the runner's built-in timer.
        limiter=self.runtime/'limiter';limiter.write_text('#!/bin/sh\nexec "$@"\n');limiter.chmod(0o755)
        self.problem=self.directory/'SET002+3.p';self.problem.write_text('cnf(a,axiom,p).')
        self.workflow=self.directory/'workflow';self.workflow.mkdir()
        rows=[dict(solver_name='Example',solver_directory=str(self.runtime),command='./solver {{problem}}',problem=str(self.problem),cores=1,threads_per_core=1,wc_limit=1,cpu_limit=1,mem_limit='64MiB',cpus='auto',exclusive_cpu='false',exclusive_node='false') for _ in range(3)]
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text(rows))
        (self.workflow/'building.json').write_text(json.dumps([dict(root=str(self.runtime),recipe='')]))
        (self.workflow/'resource-limiter.txt').write_text(str(limiter)+' {{solver_command}}')
        (self.workflow/'Makefile').write_text('SLURMY_PARTITION := local\nBATCH_SIZE := 1\nGET_COMPUTER_INFO := false\n')
        # Any accidental remote invocation fails and leaves proof in this marker.
        binary=self.directory/'bin';binary.mkdir();self.marker=self.directory/'remote-invoked'
        for name in ('ssh','scp','sbatch','squeue','sacct','scontrol'):
            tool=binary/name;tool.write_text('#!/bin/sh\ntouch '+str(self.marker)+'\nexit 99\n');tool.chmod(0o755)
        self.paths=patch.dict(os.environ,PATH=str(binary)+os.pathsep+os.environ['PATH']);self.paths.start();self.addCleanup(self.paths.stop)
        local.save_settings(dict(max_cores=1,max_parallel_batches=1,memory_mib=128))
        self.identifiers=[];self.addCleanup(self.cleanup_jobs)

    def cleanup_jobs(self):
        for identifier in self.identifiers:
            if local.job_path(identifier).exists():local.action('delete',identifier)
        self.assertFalse(self.marker.exists(),'Local backend invoked a remote tool')

    def wait_state(self,identifier,states):
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            state=local.read_json(local.job_path(identifier)/'local-state.json')
            if state.get('state') in states:return state
            time.sleep(.05)
        self.fail('Local job did not reach '+str(states)+': '+str(state))

    def run_job(self):
        local.prepare(self.workflow);identifier=local.dispatch(self.workflow);self.identifiers.append(identifier)
        return identifier

    def test_complete_results_outputs_and_all_download_formats(self):
        identifier=self.run_job();self.wait_state(identifier,{'done','incomplete'})
        job=collect(identifier).find_job(identifier)
        self.assertEqual(job.state,'DONE');self.assertEqual(job.completed,3)
        self.assertEqual({r.szs_status for r in job.results.values()},{'Theorem'})
        from slurmy_web.monitor import RemoteCollector
        output=RemoteCollector('local',1).fetch_task_output(identifier,job.results[0])
        self.assertIn('% SZS status Theorem',str(output.streams))
        sys.path.insert(0,str(local.REPO/'web-app'));import download
        for mode in download.MODES:
            self.assertTrue(download.download(identifier,mode,host='local',output=self.directory/('download-'+mode)),mode)
        first=self.directory/'incremental';self.assertTrue(download.download(identifier,host='local',output=first,incremental=True))
        with next(first.glob('*.csv')).open() as stream:self.assertEqual(list(csv.DictReader(stream)),[])
        self.assertEqual(download.list_jobs(host='local')[0].id,identifier)

    def test_stop_resume_preserves_finished_pairs_and_committed_inputs(self):
        self.solver.write_text('#!/bin/sh\nsleep .3\necho "% SZS status Theorem"\n')
        identifier=self.run_job()
        deadline=time.monotonic()+10
        while len([r for r in local.results(local.job_path(identifier)).values() if r['complete']=='true'])<1 and time.monotonic()<deadline:time.sleep(.03)
        local.action('stop',identifier)
        state=local.read_json(local.job_path(identifier)/'local-state.json');self.assertEqual(state['state'],'stopped')
        before={key:row['archive'] for key,row in local.results(local.job_path(identifier)).items() if row['complete']=='true'}
        self.solver.unlink() # Resume must use the committed executable, not live workflow inputs.
        local.action('resume',identifier);self.wait_state(identifier,{'done','incomplete'})
        job=collect(identifier).find_job(identifier);self.assertEqual(job.completed,3,job.controller_log)
        self.assertEqual({key:job.results[key].archive for key in before},before)

    def test_local_build_and_reuse_when_source_is_unchanged(self):
        source=self.directory/'source';source.mkdir()
        (source/'solver.in').write_text('#!/bin/sh\necho "% SZS status Theorem"\n')
        recipe=source/'build.sh';recipe.write_text('#!/bin/sh\ncp solver.in "$SLURMY_BUILD_OUTPUT/solver"\nchmod +x "$SLURMY_BUILD_OUTPUT/solver"\nprintf "%s" "$CMAKE_BUILD_PARALLEL_LEVEL" > "$SLURMY_BUILD_OUTPUT/build-cores"\n')
        (self.workflow/'building.json').write_text(json.dumps([dict(root=str(self.runtime),source=str(source),recipe=str(recipe),artifact='solver',build_policy='changed')]))
        first=self.run_job();self.wait_state(first,{'done','incomplete'})
        self.assertEqual(collect(first).find_job(first).completed,3)
        self.assertEqual((self.runtime/'build-cores').read_text(),'1')
        second=local.dispatch(self.workflow);self.identifiers.append(second);self.wait_state(second,{'done','incomplete'})
        self.assertIn('Reusing local build', (local.job_path(second)/'logs/submission-controller.log').read_text())
        (source/'solver.in').write_text('#!/bin/sh\necho "% SZS status Satisfiable"\n')
        third=local.dispatch(self.workflow);self.identifiers.append(third);self.wait_state(third,{'done','incomplete'})
        self.assertEqual({row.szs_status for row in collect(third).find_job(third).results.values()},{'Satisfiable'})

    def test_python_build_recipe_without_executable_bit(self):
        source=self.directory/'python-source';source.mkdir()
        recipe=source/'build.py'
        executable='#!/bin/sh\necho "% SZS status Theorem"\n'
        recipe.write_text('#!'+sys.executable+'\nimport os,pathlib\nroot=pathlib.Path(os.environ["SLURMY_BUILD_OUTPUT"])\n(root/"solver").write_text('+repr(executable)+')\n(root/"solver").chmod(0o755)\n')
        (self.workflow/'building.json').write_text(json.dumps([dict(root=str(self.runtime),source=str(source),recipe=str(recipe),artifact='solver')]))
        identifier=self.run_job();self.wait_state(identifier,{'done','incomplete'})
        job=collect(identifier).find_job(identifier)
        self.assertEqual(job.completed,3,job.controller_log)

    def test_two_jobs_share_the_cpu_budget(self):
        self.solver.write_text('#!/bin/sh\nsleep .1\necho "% SZS status Theorem"\n')
        local.save_settings(dict(max_parallel_batches=2))
        first=self.run_job();second=local.dispatch(self.workflow);self.identifiers.append(second)
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            states=[local.read_json(local.job_path(identifier)/'local-state.json') for identifier in [first,second]]
            cores=[core for state in states for task in state.get('active',{}).values() for core in task['cores']]
            self.assertLessEqual(len(cores),1)
            if all(state.get('state') in {'done','incomplete'} for state in states):break
            time.sleep(.02)
        self.assertEqual([collect(identifier).find_job(identifier).completed for identifier in [first,second]],[3,3])

    def test_real_runsolver_wall_limit_and_native_result_statistics(self):
        limiter=local.REPO/'implementation/build/runsolver/runsolver'
        if not limiter.is_file():self.skipTest('Bundled runsolver definition is unavailable')
        shutil.copy2(limiter,self.runtime/'runsolver')
        self.solver.write_text('#!/bin/sh\nsleep 5\n')
        (self.workflow/'resource-limiter.txt').write_text(str(self.runtime/'runsolver')+' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} --timestamp --add-eof {{solver_command}}')
        with (self.workflow/'solver-problem-pairs.csv').open() as stream:rows=list(csv.DictReader(stream))
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text(rows[:1]))
        identifier=self.run_job();self.wait_state(identifier,{'done','incomplete'})
        result=collect(identifier).find_job(identifier).results[0]
        self.assertTrue(result.complete);self.assertEqual(result.status,'time-limit')
        self.assertIsNotNone(result.cpu_seconds);self.assertIsNotNone(result.wall_seconds)

    def test_blank_host_uses_local_routes_and_settings(self):
        with patch.object(workflows,'migrate_user_runs'):client=create_app().test_client()
        with client.session_transaction() as session:session['csrf']='local-test'
        response=client.get('/new?host=');self.assertEqual(response.status_code,200)
        self.assertIn('data-host="local"',response.text);self.assertIn('placeholder="(local backend)"',response.text)
        self.assertNotIn('value="datalab" required',response.text)
        self.assertEqual(client.get('/api/connection?host=').json['mode'],'local')
        for endpoint in ['/api/jobs?host=','/api/local-status?host=','/cluster?host=','/api/cluster?host=','/api/cluster/node?host=']:
            self.assertEqual(client.get(endpoint).status_code,200,endpoint)
        response=client.post('/api/connection/test?host=',json={},headers={'X-CSRF-Token':'local-test'})
        self.assertEqual(response.status_code,200,response.json)

    def test_editor_save_without_axioms_and_portable_local_export(self):
        from slurmy_web import portable
        data=dict(name='local-editor',host='local',partition='CPU-amd',mode='interactive',batch_size=2,max_outstanding=50,
                  configurations=[dict(solver_name='Local solver',solver_directory=str(self.runtime),command='./solver {{problem}}',cores=1,threads_per_core=1,wc_limit=1,cpu_limit=1,mem_limit='64MiB',cpus='auto',exclusive_cpu='false',exclusive_node='false')],
                  globs=[str(self.problem)],axiom_mode='custom',axiom_globs=[],building_mode='create',resources=[dict(root=str(self.runtime),mode='none')],
                  limiter_mode='inline',limiter=str(self.runtime/'limiter')+' {{solver_command}}',get_computer_info=False)
        with patch.object(workflows,'USER_RUNS',self.directory),patch.object(workflows,'migrate_user_runs'):
            client=create_app().test_client()
            with client.session_transaction() as session:session['csrf']='local-editor'
            response=client.post('/api/workflows',json=data,headers={'X-CSRF-Token':'local-editor'})
            self.assertEqual(response.status_code,201,response.json)
            saved=Path(response.json['directory'])
            with portable.export_bundle(saved,'local') as bundle:
                imported,count=portable.import_bundle(bundle,'local-restored',selected_host='local')
            self.assertEqual(count,1)
            self.assertTrue((imported/'solver-problem-pairs.csv').is_file())

    def test_prepare_dispatch_monitor_download_and_delete_through_api(self):
        import re
        from slurmy_web import app as web
        with patch.object(workflows,'USER_RUNS',self.directory),patch.object(workflows,'migrate_user_runs'),patch.object(web,'STATE',self.directory/'web-state'):
            client=create_app().test_client()
            with client.session_transaction() as session:session['csrf']='local-api'
            query={'directory':str(self.workflow),'host':'local'};headers={'X-CSRF-Token':'local-api'}
            response=client.post('/api/workflow/action',query_string=query,json={'action':'prepare-submit'},headers=headers)
            self.assertEqual(response.status_code,202,response.json)
            key=response.json['id'];deadline=time.monotonic()+20
            while time.monotonic()<deadline:
                operation=client.get('/api/operations/'+key).json
                if operation['state']!='running':break
                time.sleep(.05)
            self.assertEqual(operation['state'],'done',operation)
            identifier=re.search(r'Slurmy job ID: ([A-Za-z0-9_.-]+)',operation['log'])[1]
            self.identifiers.append(identifier);self.wait_state(identifier,{'done','incomplete'})
            response=client.get('/api/jobs/'+identifier,query_string={'host':'local','fresh':'1'})
            self.assertEqual(response.status_code,200,response.json)
            self.assertEqual(response.json['completed'],3)
            response=client.get('/api/jobs/'+identifier+'/output/0',query_string={'host':'local'})
            self.assertEqual(response.status_code,200,response.json)
            response=client.post('/api/jobs/'+identifier+'/name',query_string={'host':'local'},json={'name':'Renamed local job'},headers=headers)
            self.assertEqual(response.status_code,200,response.json)
            # A locally prepared wrapper must never be dispatched as a cluster submission.
            response=client.post('/api/workflow/action',query_string={**query,'host':'datalab'},json={'action':'submit'},headers=headers)
            self.assertEqual(response.status_code,400,response.json)
            self.assertIn('local backend',response.json['error'])
            response=client.post('/api/jobs/'+identifier+'/delete',query_string={'host':'local'},json={},headers=headers)
            self.assertEqual(response.status_code,202,response.json)
            key=response.json['id'];deadline=time.monotonic()+10
            while time.monotonic()<deadline:
                operation=client.get('/api/operations/'+key).json
                if operation['state']!='running':break
                time.sleep(.05)
            self.assertEqual(operation['state'],'done',operation)
            self.assertFalse(local.job_path(identifier).exists())

    def test_local_tptp_mapping_and_thread_validation(self):
        from slurmy_archives import expand_cluster_tptp,browse_cluster_tptp,expand_axiom_source
        base=self.directory/'TPTP';folder=base/'TPTP-v10.0.0/Problems/SET';folder.mkdir(parents=True)
        (folder/'SET001+1.p').write_text('cnf(a,axiom,p).')
        axioms=base/'TPTP-v10.0.0/Axioms';axioms.mkdir();(axioms/'SET.ax').write_text('cnf(a,axiom,p).')
        local.save_settings(dict(tptp_root=str(base)))
        matches=expand_cluster_tptp('tptp://TPTP-v10.0.0/Problems/SET/*.p','local')
        self.assertEqual(matches,[str(folder/'SET001+1.p')])
        self.assertTrue(browse_cluster_tptp('tptp://','local')['items'])
        self.assertEqual(expand_axiom_source('tptp://TPTP-v10.0.0/Axioms/*.ax','local')[0][0],axioms/'SET.ax')
        from check_threads import check_requested
        self.assertGreaterEqual(check_requested('local','unused',1),1)
        with self.assertRaises(ValueError):check_requested('local','unused',10000)
