"""Exercise stop/delete replay against isolated fake SLURM commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

TEMPLATES=Path(__file__).parent/'templates'


class DeleteReplayTests(unittest.TestCase):
    def test_delete_acknowledgment_can_be_replayed_and_local_safety_is_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            tools=root/'tools';tools.mkdir()
            queue=tools/'squeue';queue.write_text('#!/bin/sh\nexit 0\n');queue.chmod(0o755)
            env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']}
            def run(action):return subprocess.run(['bash',str(TEMPLATES/f'remote_{action}.sh'),'job'],env=env,capture_output=True,text=True)
            self.assertNotEqual(run('delete').returncode,0)
            self.assertTrue(job.exists())
            self.assertEqual(run('stop').returncode,0)
            self.assertEqual(run('delete').returncode,0)
            self.assertFalse(job.exists())
            self.assertEqual(run('delete').returncode,0)
            self.assertEqual(run('stop').returncode,0)

    def test_an_unfinished_deletion_can_continue_after_its_stop_marker_was_removed(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            (job/'remaining-file').write_text('partial deletion')
            locks=root/'slurmy/.delete-locks';locks.mkdir();(locks/'job.deleting').touch()
            tools=root/'tools';tools.mkdir()
            queue=tools/'squeue';queue.write_text('#!/bin/sh\nexit 0\n');queue.chmod(0o755)
            result=subprocess.run(['bash',str(TEMPLATES/'remote_delete.sh'),'job'],
                                  env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']},capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertFalse(job.exists())

    def test_cancellation_race_rechecks_queue_instead_of_aborting_stop(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            tools=root/'tools';tools.mkdir()
            (root/'queued').touch()
            queue=tools/'squeue';queue.write_text('#!/bin/sh\nif test -f "$HOME/queued"; then printf "123|%s/slurmy/jobs/job\\n" "$HOME"; fi\n');queue.chmod(0o755)
            cancel=tools/'scancel';cancel.write_text('#!/bin/sh\nrm "$HOME/queued"\nexit 1\n');cancel.chmod(0o755)
            sleep=tools/'sleep';sleep.write_text('#!/bin/sh\nexit 0\n');sleep.chmod(0o755)
            result=subprocess.run(['bash',str(TEMPLATES/'remote_stop.sh'),'job'],
                                  env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']},capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertTrue(job.exists())

    def test_real_remote_side_effects_can_lose_both_stop_and_delete_replies(self):
        import importlib.util
        from types import SimpleNamespace
        from unittest.mock import patch
        spec=importlib.util.spec_from_file_location('retry_job_cli',Path(__file__).with_name('slurmy-job.py'))
        cli=importlib.util.module_from_spec(spec);spec.loader.exec_module(cli)
        real_run=subprocess.run
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            (job/'saved-result').write_text('saved result')
            sibling=root/'slurmy/jobs/other';sibling.mkdir();(sibling/'keep').write_text('unrelated job')
            tools=root/'tools';tools.mkdir()
            queue=tools/'squeue';queue.write_text('#!/bin/sh\nexit 0\n');queue.chmod(0o755)
            env={**os.environ,'HOME':folder,'PATH':str(tools)+os.pathsep+os.environ['PATH']}
            counts={}
            def transport(*args,stdin,**kwargs):
                name=Path(stdin.name).name;counts[name]=counts.get(name,0)+1
                result=real_run(['bash','-s','--','job'],input=stdin.read(),env=env,capture_output=True)
                self.assertEqual(result.returncode,0,result.stderr)
                # The remote operation completed, but its first SSH reply was lost.
                return SimpleNamespace(returncode=255 if counts[name]==1 else 0)
            with patch('sys.argv',['slurmy-job.py','delete','job']),patch.object(cli.subprocess,'run',side_effect=transport),patch.object(cli.time,'sleep'):
                self.assertEqual(cli.main(),0)
            self.assertEqual(counts,{'remote_stop.sh':2,'remote_delete.sh':2})
            self.assertFalse(job.exists())
            self.assertEqual((sibling/'keep').read_text(),'unrelated job')

    def test_symlink_job_is_never_followed_or_treated_as_already_deleted(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);jobs=root/'slurmy/jobs';jobs.mkdir(parents=True)
            outside=root/'outside';outside.mkdir();(outside/'keep').write_text('keep');(outside/'.stopped').touch()
            (jobs/'job').symlink_to(outside,target_is_directory=True)
            for action in ('stop','delete'):
                result=subprocess.run(['bash',str(TEMPLATES/f'remote_{action}.sh'),'job'],env={**os.environ,'HOME':folder},capture_output=True)
                self.assertNotEqual(result.returncode,0)
            self.assertEqual((outside/'keep').read_text(),'keep')
            self.assertTrue((jobs/'job').is_symlink())
