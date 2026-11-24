"""File replacement regenerates editor decisions and derivatives as one change."""
import csv
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from slurmy_web import workflows, workflow_files
from slurmy_web.app import create_app


class WorkflowFileTests(unittest.TestCase):
    def test_json_definition_and_generated_configuration_edits_update_each_other(self):
        definition=json.loads((self.directory/'workflow.json').read_text())
        self.assertNotIn('host',definition)
        self.assertNotIn('SLURMY_HOST',(self.directory/'Makefile').read_text())
        definition['configurations'][0]['cpu_limit']='7'
        workflow_files.replace(self.directory,'workflow.json',json.dumps(definition))
        self.assertEqual(self.rows('configurations.csv')[0]['cpu_limit'],'7')
        self.assertEqual(self.rows('jobpairs.csv')[0]['cpu_limit'],'7')
        rows=self.rows('configurations.csv');rows[0]['cpu_limit']='8'
        workflow_files.replace(self.directory,'configurations.csv',workflows.csv_text(rows,tuple(rows[0])))
        self.assertEqual(json.loads((self.directory/'workflow.json').read_text())['configurations'][0]['cpu_limit'],'8')

    def test_postprocessor_persists_across_json_csv_and_http_replacement(self):
        post=self.root/'postprocessing';post.mkdir()
        (post/'parse.py').write_text('print("{}")')
        definition=json.loads((self.directory/'workflow.json').read_text())
        definition['configurations'][0].update(postprocessor_override=True,postprocessor_mode='custom',postprocessor_directory=str(post),
            postprocessor_command='python3 parse.py',postprocessor_timeout=42)
        client=create_app().test_client()
        with client.session_transaction() as session:session['csrf']='post-token'
        response=client.post('/api/workflows/files/replace',query_string={'directory':str(self.directory)},
            headers={'X-CSRF-Token':'post-token'},data={'file':'workflow.json',
                'replacement':(io.BytesIO(json.dumps(definition).encode()),'workflow.json')})
        self.assertEqual(response.status_code,200,response.get_json())
        self.assertEqual(self.rows('configurations.csv')[0]['postprocessor_command'],'python3 parse.py')
        self.assertEqual(self.rows('jobpairs.csv')[0]['postprocessor_timeout'],'42')
        rows=self.rows('configurations.csv');rows[0].update(postprocessor_mode='none',postprocessor_timeout='30')
        workflow_files.replace(self.directory,'configurations.csv',workflows.csv_text(rows,tuple(rows[0])))
        self.assertEqual(workflows.decisions(self.directory)['configurations'][0]['postprocessor_mode'],'none')
        self.assertEqual(self.rows('jobpairs.csv')[0]['postprocessor_mode'],'none')

    def test_workflow_default_resolves_csv_and_override_survives_regeneration(self):
        post=self.root/'post';post.mkdir();(post/'parse.py').write_text('print("{}")')
        data=workflows.decisions(self.directory)
        data['postprocessor']=dict(mode='custom',directory=str(post),command='python3 parse.py',timeout=50)
        first=data['configurations'][0];first['postprocessor_override']=False
        data['configurations'].append({**first,'solver_name':'Override','postprocessor_override':True,
                                      'postprocessor_mode':'none','postprocessor_timeout':30})
        workflows.save(data,self.directory)
        rows=self.rows('configurations.csv')
        self.assertEqual([r['postprocessor_mode'] for r in rows],['custom','none'])
        self.assertEqual(rows[0]['postprocessor_timeout'],'50')
        self.assertNotIn('postprocessor_override',rows[0])
        saved=workflows.decisions(self.directory)
        self.assertFalse(saved['configurations'][0]['postprocessor_override'])
        self.assertTrue(saved['configurations'][1]['postprocessor_override'])
        saved['postprocessor']['timeout']=80;workflows.save(saved,self.directory)
        self.assertEqual(self.rows('jobpairs.csv')[0]['postprocessor_timeout'],'80')
        self.assertEqual(workflows.copyable_solver_configurations(self.directory)[0]['postprocessor_timeout'],'80')
        self.assertTrue(workflows.copyable_solver_configurations(self.directory)[0]['postprocessor_override'])
        # Importing resolved CSV treats every row as an explicit override.
        workflow_files.replace(self.directory,'configurations.csv',(self.directory/'configurations.csv').read_text())
        self.assertTrue(all(c['postprocessor_override'] for c in workflows.decisions(self.directory)['configurations']))

    def test_invalid_postprocessor_never_replaces_definition(self):
        original=self.snapshot()
        data=workflows.decisions(self.directory)
        data['configurations'][0].update(postprocessor_override=True,postprocessor_mode='custom',postprocessor_directory=str(self.root),
                                       postprocessor_command='python3 missing.py')
        with self.assertRaisesRegex(ValueError,'existing file'):
            workflows.save(data,self.directory)
        self.assertEqual(self.snapshot(),original)

    def test_legacy_hidden_json_is_read_and_new_saves_publish_visible_json(self):
        path=self.directory/'workflow.json';legacy=self.directory/'.slurmy-workflow.json'
        path.rename(legacy)
        decisions=workflows.decisions(self.directory)
        workflows.save(decisions,self.directory)
        self.assertTrue(path.is_file())
        self.assertEqual(workflows.decisions(self.directory)['name'],self.directory.name)

    def test_build_script_arguments_are_preserved_in_generated_builds(self):
        import shlex
        from slurmy import read_builds,generate
        script=self.runtime/'build python.py';script.write_text('#!/usr/bin/env python3\nprint("build")\n')
        data=workflows.decisions(self.directory)
        data['resources'][0].update(mode='script',script=shlex.join([str(script),'--target','name with spaces']),artifact='solver')
        workflows.save(data,self.directory)
        resource,=read_builds(self.directory/'building.txt')
        self.assertEqual(resource.arguments,('--target','name with spaces'))
        generate(self.directory/'jobpairs.csv',self.directory/'building.txt',self.directory/'resource_limiter_template.txt')
        command=(self.directory/'submit.sh.files/builds/run.sh').read_text()
        self.assertIn('--recipe-arg=--target',command)
        self.assertIn("--recipe-arg='name with spaces'",command)
        duplicate=workflows.duplicate(self.directory,'duplicate')
        copied,=read_builds(duplicate/'building.txt')
        self.assertEqual(copied.recipe,script)
        self.assertEqual(copied.arguments,resource.arguments)

    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.users=self.root/'users';self.users.mkdir()
        self.runtime=self.root/'runtime';self.runtime.mkdir()
        (self.runtime/'solver').touch();(self.runtime/'limiter').touch()
        self.problem=self.root/'first.p';self.problem.write_text('cnf(a,axiom,p).')
        for name,value in [('USER_RUNS',self.users),('BASE',self.users)]:
            p=patch.object(workflows,name,value);p.start();self.addCleanup(p.stop)
        p=patch.object(workflows,'check_requested');p.start();self.addCleanup(p.stop)
        data=dict(name='test',host='datalab',partition='CPU-amd',batch_size=2,max_outstanding=3,
                  mode='interactive',configurations=[dict(solver_name='First',command='./solver {{problem}}',
                  solver_directory=str(self.runtime),wc_limit=1,cpu_limit=1,mem_limit='1GiB',cores=1,
                  threads_per_core=1,exclusive_cpu='false',exclusive_node='false')],globs=[str(self.problem)],
                  building_mode='create',resources=[dict(root=str(self.runtime),mode='none')],
                  limiter_mode='inline',limiter=str(self.runtime/'limiter')+' {{solver_command}}')
        self.directory,_=workflows.save(data)

    def rows(self,name):
        return list(csv.DictReader(io.StringIO((self.directory/name).read_text())))

    def snapshot(self):
        return {p.name:p.read_bytes() for p in self.directory.iterdir() if p.is_file()}

    def test_configurations_replacement_updates_pairs_editor_and_preparation(self):
        configs=self.rows('configurations.csv');configs[0].update(solver_name='New',cpu_limit='2')
        (self.directory/'submit.sh').write_text('old prepared job')
        workflow_files.replace(self.directory,'configurations.csv',workflows.csv_text(configs,tuple(configs[0])))
        self.assertEqual(self.rows('jobpairs.csv')[0]['solver_name'],'New')
        self.assertEqual(workflows.decisions(self.directory)['configurations'][0]['cpu_limit'],'2')
        self.assertFalse((self.directory/'submit.sh').exists())
        self.assertFalse(list(self.directory.glob('.replace-*')))

    def test_explicit_pairs_switches_source_and_removes_stale_configurations(self):
        rows=self.rows('jobpairs.csv');rows[0]['solver_name']='Explicit'
        workflow_files.replace(self.directory,'jobpairs.csv',workflows.csv_text(rows))
        data=workflows.decisions(self.directory)
        self.assertEqual(data['mode'],'jobpairs')
        self.assertEqual(data['jobpairs'],str(self.directory/'jobpairs.csv'))
        self.assertFalse((self.directory/'configurations.csv').exists())
        self.assertFalse((self.directory/'problem-globs.txt').exists())
        # A later ordinary editor save must preserve the replaced rows.
        workflows.save(data,self.directory)
        self.assertEqual(self.rows('jobpairs.csv')[0]['solver_name'],'Explicit')

    def test_problem_globs_rebuild_cartesian_product_and_use_workflow_relative_paths(self):
        (self.directory/'extra.p').write_text('cnf(a,axiom,p).')
        workflow_files.replace(self.directory,'problem-globs.txt',str(self.problem)+'\nextra.p\n')
        self.assertEqual(len(self.rows('jobpairs.csv')),2)
        self.assertEqual(workflows.decisions(self.directory)['globs'][-1],str(self.directory/'extra.p'))

    def test_axioms_limiter_and_builds_update_editor(self):
        axiom=self.root/'custom.ax';axiom.write_text('cnf(a,axiom,p).')
        # Add this supported optional generated file as an existing workflow would.
        (self.directory/'axiom-globs.txt').write_text(str(axiom))
        workflow_files.replace(self.directory,'axiom-globs.txt',str(axiom))
        self.assertEqual(workflows.decisions(self.directory)['axiom_mode'],'custom')
        self.assertIn('AXIOMS := axiom-globs.txt',(self.directory/'Makefile').read_text())
        limiter=str(self.runtime/'limiter')+' --cpu-limit {{cpu_limit}} {{solver_command}}'
        workflow_files.replace(self.directory,'resource_limiter_template.txt',limiter)
        self.assertEqual(workflows.decisions(self.directory)['limiter'],limiter)
        recipe=self.root/'build.sh';recipe.write_text('echo build')
        body=json.dumps([dict(root=str(self.runtime),source=str(self.runtime),recipe=str(recipe),artifact='solver',build_policy='changed')])
        workflow_files.replace(self.directory,'building.txt',body)
        resource=workflows.decisions(self.directory)['resources'][0]
        self.assertEqual(resource['script'],str(recipe));self.assertEqual(resource['build_policy'],'changed')

    def test_problem_filename_selection_is_saved_and_regenerated_consistently(self):
        later=self.root/'later.p';later.write_text('cnf(b,axiom,q).')
        data=workflows.decisions(self.directory)
        data.update(globs=[str(self.root/'*.p')],problem_selections=['later.p\nfirst.p'])
        workflows.save(data,self.directory)
        self.assertEqual([Path(row['problem']).name for row in self.rows('jobpairs.csv')],['later.p','first.p'])
        self.assertIn('--problem-selection-file problem-selection.json',(self.directory/'Makefile').read_text())
        workflow_files.replace(self.directory,'problem-selection.json','["first.p"]')
        self.assertEqual([Path(row['problem']).name for row in self.rows('jobpairs.csv')],['first.p'])
        self.assertEqual(workflows.decisions(self.directory)['problem_selections'],['first.p'])

    def test_makefile_settings_update_workflow_and_custom_rules_are_rejected(self):
        body=(self.directory/'Makefile').read_text().replace('BATCH_SIZE := 2','BATCH_SIZE := 7').replace('CPU-amd','CPU-intel')
        workflow_files.replace(self.directory,'Makefile',body)
        data=workflows.decisions(self.directory)
        self.assertEqual(str(data['batch_size']),'7');self.assertEqual(data['partition'],'CPU-intel')
        before=self.snapshot()
        with self.assertRaisesRegex(ValueError,'custom commands'):
            workflow_files.replace(self.directory,'Makefile',body+'\ncustom:\n\techo custom\n')
        self.assertEqual(self.snapshot(),before)

    def test_computer_information_setting_can_be_replaced_and_saved(self):
        body = (self.directory/'Makefile').read_text().replace('GET_COMPUTER_INFO := true', 'GET_COMPUTER_INFO := false')
        workflow_files.replace(self.directory, 'Makefile', body)
        data = workflows.decisions(self.directory)
        self.assertIs(data['get_computer_info'], False)
        workflows.save(data, self.directory)
        self.assertIn('GET_COMPUTER_INFO := false', (self.directory/'Makefile').read_text())
        before = self.snapshot()
        with self.assertRaisesRegex(ValueError, 'GET_COMPUTER_INFO'):
            workflow_files.replace(self.directory, 'Makefile', body.replace('GET_COMPUTER_INFO := false', 'GET_COMPUTER_INFO := invalid'))
        self.assertEqual(self.snapshot(), before)

    def test_normal_save_removes_obsolete_sources_and_axiom_configuration(self):
        data=workflows.decisions(self.directory)
        axiom=self.root/'old.ax';axiom.write_text('cnf(a,axiom,p).')
        data.update(axiom_mode='custom',axiom_globs=[str(axiom)])
        workflows.save(data,self.directory)
        self.assertTrue((self.directory/'axiom-globs.txt').exists())
        (self.directory/'submit.sh').write_text('stale prepared input')
        data.update(axiom_mode='cluster',axiom_globs=[],mode='jobpairs',jobpairs=str(self.directory/'jobpairs.csv'))
        workflows.save(data,self.directory)
        for name in ['axiom-globs.txt','configurations.csv','problem-globs.txt','submit.sh']:
            self.assertFalse((self.directory/name).exists(),name)
        self.assertEqual(workflows.decisions(self.directory)['mode'],'jobpairs')

    def test_makefile_append_assignment_is_not_silently_reinterpreted(self):
        before=self.snapshot()
        body=(self.directory/'Makefile').read_text().replace('BATCH_SIZE := 2','BATCH_SIZE += 7')
        with self.assertRaisesRegex(ValueError,'custom commands'):
            workflow_files.replace(self.directory,'Makefile',body)
        self.assertEqual(self.snapshot(),before)

    def test_invalid_replacement_and_failed_commit_preserve_originals(self):
        before=self.snapshot()
        with self.assertRaises(ValueError):workflow_files.replace(self.directory,'configurations.csv','wrong,headers\n1,2\n')
        self.assertEqual(self.snapshot(),before)
        real=Path.replace
        def fail(path,target):
            if target.name=='configurations.csv':raise OSError('disk failure')
            return real(path,target)
        with patch.object(Path,'replace',fail),self.assertRaisesRegex(OSError,'disk failure'):
            workflow_files.replace(self.directory,'problem-globs.txt',str(self.problem))
        self.assertEqual(self.snapshot(),before)

    def test_examples_and_symlinks_cannot_be_replaced(self):
        content=(self.directory/'problem-globs.txt').read_text()
        with patch.object(workflows,'USER_RUNS',self.root/'somewhere-else'),self.assertRaisesRegex(ValueError,'Duplicate an example'):
            workflow_files.replace(self.directory,'problem-globs.txt',content)
        target=self.directory/'problem-globs.txt';target.unlink();target.symlink_to(self.problem)
        with self.assertRaisesRegex(ValueError,'symbolic link'):
            workflow_files.replace(self.directory,'problem-globs.txt',content)
        self.assertEqual(self.problem.read_text(),'cnf(a,axiom,p).')

    def test_download_replace_and_path_restrictions_over_http(self):
        with patch.object(workflows,'migrate_user_runs'):
            app=create_app()
        client=app.test_client()
        with client.session_transaction() as session:session['csrf']='test'
        query={'directory':str(self.directory),'file':'problem-globs.txt'}
        response=client.get('/api/workflows/files/download',query_string=query)
        self.assertEqual(response.status_code,200)
        self.assertEqual(response.data,(self.directory/'problem-globs.txt').read_bytes());response.close()
        self.assertIn('attachment',response.headers['Content-Disposition'])
        for filename in ['../Makefile','.slurmy-workflow.json','submit.sh']:
            self.assertEqual(client.get('/api/workflows/files/download',query_string={**query,'file':filename}).status_code,400)
        self.assertEqual(client.post('/api/workflows/files/replace',query_string={'directory':str(self.directory)}).status_code,403)
        options=dict(query_string={'directory':str(self.directory)},headers={'X-CSRF-Token':'test'})
        response=client.post('/api/workflows/files/replace',data={'file':'problem-globs.txt','replacement':(io.BytesIO(str(self.problem).encode()),'chosen.txt')},**options)
        self.assertEqual(response.status_code,200,response.json)
        response=client.post('/api/workflows/files/replace',data={'file':'problem-globs.txt','replacement':(io.BytesIO(b'\xff'),'bad.txt')},**options)
        self.assertEqual(response.status_code,400)
        html=client.get('/workflow',query_string={'directory':str(self.directory)}).text
        self.assertEqual(html.count('class="secondary replace-workflow-file"'),len([n for n in workflow_files.FILES if (self.directory/n).is_file()]))
