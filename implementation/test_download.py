"""Public download wrapper: exact identity, silence, CLI modes and Rich selection."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import download as api


class DownloadTests(unittest.TestCase):
    def setUp(self):
        self.jobs=[api.Job('new_1795517000_2','Workflow (1)',1795517000),
                   api.Job('old_1795516000_1','Workflow',1795516000)]

    def test_function_maps_modes_to_same_engine_and_is_silent(self):
        for mode,flag in [('default',None),('verbose','--verbose'),('starexec','--starexec')]:
            with self.subTest(mode=mode),patch.object(api,'list_jobs',return_value=self.jobs),patch.object(api.subprocess,'run',return_value=subprocess.CompletedProcess([],0)) as run:
                stdout,stderr=io.StringIO(),io.StringIO()
                with contextlib.redirect_stdout(stdout),contextlib.redirect_stderr(stderr):
                    result=api.download('Workflow (1)',mode,host='datalab',output='relative results')
                self.assertIs(result,True);self.assertEqual(stdout.getvalue()+stderr.getvalue(),'')
                argv=run.call_args.args[0]
                self.assertEqual(argv[:3],[sys.executable,str(ROOT/'implementation/slurmy-sync.py'),self.jobs[0].id])
                self.assertEqual('--verbose' in argv,flag=='--verbose')
                self.assertEqual('--starexec' in argv,flag=='--starexec')
                self.assertEqual(argv[argv.index('--output')+1],str(Path('relative results').absolute()))
                self.assertEqual(run.call_args.kwargs['cwd'],ROOT)
                self.assertEqual(run.call_args.kwargs['stdout'],subprocess.DEVNULL)
                self.assertEqual(run.call_args.kwargs['stderr'],subprocess.DEVNULL)

    def test_id_takes_precedence_and_duplicate_names_are_not_guessed(self):
        duplicate=api.Job('third_1795515000_3','Workflow',1795515000)
        jobs=self.jobs+[duplicate]
        self.assertEqual(api._resolve(jobs,self.jobs[1].id,False),self.jobs[1])
        with self.assertRaisesRegex(api.DownloadError,'ambiguous'):api._resolve(jobs,'Workflow',False)
        with self.assertRaisesRegex(api.DownloadError,'No job'):api._resolve(jobs,'missing',False)
        self.assertEqual(api._resolve(jobs,None,True),self.jobs[0])

    def test_failure_returns_false_without_printing_or_starting_transfer(self):
        for failure in [OSError('offline'),api.DownloadError('ambiguous'),subprocess.TimeoutExpired('ssh',10),ValueError('bad list')]:
            with self.subTest(failure=failure),patch.object(api,'list_jobs',side_effect=failure),patch.object(api.subprocess,'run') as run:
                stdout,stderr=io.StringIO(),io.StringIO()
                with contextlib.redirect_stdout(stdout),contextlib.redirect_stderr(stderr):self.assertIs(api.download('Workflow'),False)
                self.assertEqual(stdout.getvalue()+stderr.getvalue(),'');run.assert_not_called()
        with patch.object(api,'list_jobs',return_value=self.jobs),patch.object(api.subprocess,'run',return_value=subprocess.CompletedProcess([],1)):
            self.assertIs(api.download('Workflow'),False)

    def test_invalid_function_options_do_not_connect(self):
        with patch.object(api,'list_jobs') as jobs:
            for options in [dict(),dict(job_name=''),dict(job_name='Workflow',mode='bad'),dict(job_name='Workflow',host='-bad'),dict(job_name='Workflow',ssh_timeout=float('nan')),dict(job_name='Workflow',ssh_timeout=None),dict(job_name='Workflow',latest=True)]:
                self.assertIs(api.download(**options),False)
            jobs.assert_not_called()

    def test_latest_and_empty_catalog(self):
        with patch.object(api,'list_jobs',return_value=self.jobs),patch.object(api.subprocess,'run',return_value=subprocess.CompletedProcess([],0)) as run:
            self.assertTrue(api.download(latest=True))
            self.assertEqual(run.call_args.args[0][2],self.jobs[0].id)
        with patch.object(api,'list_jobs',return_value=[]):self.assertFalse(api.download(latest=True))

    def test_catalog_uses_shared_transport_and_validates_response(self):
        rows=[dict(id=j.id,name=j.name,submitted=j.submitted) for j in reversed(self.jobs)]
        with patch.object(api,'connection_options',return_value=['-o','ControlMaster=auto']),patch.object(api.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(rows),'')) as run:
            self.assertEqual(api.list_jobs(host='test-host'),self.jobs)
            self.assertIn('ControlMaster=auto',run.call_args.args[0]);self.assertIn('test-host',run.call_args.args[0])
            self.assertEqual(run.call_args.kwargs['input'],api.REMOTE_JOBS)
        for text in ['not JSON','{}','[{}]',json.dumps([rows[0],rows[0]])]:
            with self.subTest(text=text),patch.object(api.subprocess,'run',return_value=subprocess.CompletedProcess([],0,text,'')):
                with self.assertRaises(api.DownloadError):api.list_jobs()

    def test_remote_catalog_ignores_staging_and_mtime_and_handles_old_names(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'slurmy/jobs';root.mkdir(parents=True)
            for identifier,metadata in [
                ('old_1795516000_1',dict(job_name='Renamed old job',created_epoch=9999999999)),
                ('new_1795517000_2',dict(job_name='Quoted [name] (1)')),
                ('legacy_1795515000',dict(job_name='legacy')),
                ('.staging-test_1999999999_1',dict(job_name='staging'))]:
                path=root/identifier;path.mkdir();(path/'metadata.json').write_text(json.dumps(metadata))
            (root/'linked_1999999999_2').symlink_to(root/'new_1795517000_2',target_is_directory=True)
            result=subprocess.run([sys.executable,'-c',api.REMOTE_JOBS],env={**os.environ,'HOME':folder},capture_output=True,text=True,check=True)
            values={row['id']:row for row in json.loads(result.stdout)}
            self.assertEqual(len(values),3)
            self.assertEqual(values['old_1795516000_1']['submitted'],1795516000)
            self.assertEqual(values['new_1795517000_2']['name'],'Quoted [name] (1)')
            self.assertEqual(values['legacy_1795515000']['submitted'],1795515000)

    def test_cli_rejects_conflicting_or_missing_selection_before_network(self):
        for argv in [[],['name','--latest'],['name','--interactive'],['--latest','--interactive'],['--latest','--mode','bad'],['--latest','--ssh-timeout','nan']]:
            with self.subTest(argv=argv),patch.object(api,'list_jobs') as jobs,contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:api.main(argv)
                self.assertEqual(error.exception.code,2);jobs.assert_not_called()

    def test_cli_arguments_latest_and_interactive_use_one_catalog(self):
        with patch.object(api,'_perform',return_value=True) as perform:
            self.assertEqual(api.main(['Workflow','--mode','verbose']),0)
            self.assertEqual(perform.call_args.args,('Workflow','verbose'))
            self.assertTrue(perform.call_args.kwargs['show_output'])
            self.assertEqual(api.main(['--latest','--mode','starexec']),0)
            self.assertTrue(perform.call_args.kwargs['latest'])
        with patch.object(api,'list_jobs',return_value=self.jobs) as jobs,patch.object(api,'_pick',return_value=(self.jobs[1],'verbose')),patch.object(api,'_perform',return_value=True) as perform:
            self.assertEqual(api.main(['--interactive']),0)
            jobs.assert_called_once();self.assertEqual(perform.call_args.kwargs['selected_job'],self.jobs[1])
            self.assertEqual(perform.call_args.args[1],'verbose')

    @unittest.skipUnless(importlib.util.find_spec('rich'),'Rich is optional')
    def test_rich_picker_selects_mode_and_exact_job_from_lists(self):
        from rich.console import Console
        output=io.StringIO()
        with patch('rich.console.Console',return_value=Console(file=output,width=120,color_system=None)),patch('rich.prompt.IntPrompt.ask',side_effect=[3,2]):
            self.assertEqual(api._pick(self.jobs),(self.jobs[1],'starexec'))
        self.assertIn('Workflow (1)',output.getvalue());self.assertIn('StarExec',output.getvalue())

    def test_missing_rich_explains_install_without_affecting_library(self):
        with patch.dict(sys.modules,{'rich.console':None}):
            with self.assertRaisesRegex(api.DownloadError,'pip install rich'):api._pick(self.jobs)
        with patch.object(api,'_perform',return_value=True):self.assertTrue(api.download('Workflow'))

    def test_cli_error_and_cancel_exit_codes(self):
        with patch.object(api,'_perform',side_effect=api.DownloadError('network failed')),contextlib.redirect_stderr(io.StringIO()) as error:
            self.assertEqual(api.main(['Workflow']),1);self.assertIn('network failed',error.getvalue())
        with patch.object(api,'_perform',side_effect=KeyboardInterrupt),contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(api.main(['Workflow']),130)
        with patch.object(api,'list_jobs',return_value=self.jobs),patch.object(api,'_pick',side_effect=EOFError),contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(api.main(['--interactive']),130)
