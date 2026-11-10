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

    def test_makefile_settings_update_workflow_and_custom_rules_are_rejected(self):
        body=(self.directory/'Makefile').read_text().replace('BATCH_SIZE := 2','BATCH_SIZE := 7').replace('CPU-amd','CPU-intel')
        workflow_files.replace(self.directory,'Makefile',body)
        data=workflows.decisions(self.directory)
        self.assertEqual(str(data['batch_size']),'7');self.assertEqual(data['partition'],'CPU-intel')
        before=self.snapshot()
        with self.assertRaisesRegex(ValueError,'custom commands'):
            workflow_files.replace(self.directory,'Makefile',body+'\ncustom:\n\techo custom\n')
        self.assertEqual(self.snapshot(),before)

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
