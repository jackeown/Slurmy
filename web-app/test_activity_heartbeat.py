"""Recovery checks avoid result reads while preserving fresh verification."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from slurmy_web.monitor import REMOTE_ACTIVITY_SCRIPT, RemoteCollector, JobSnapshot, ClusterSnapshot
from slurmy_web.recovery_watch import RecoveryWatch


class ActivityHeartbeatTests(unittest.TestCase):
    def run_activity(self, root, output='', code=0):
        tools=root/'tools';tools.mkdir(exist_ok=True)
        queue=tools/'squeue';queue.write_text('#!/bin/sh\nprintf "%s\\n" '+repr(output)+'\nexit '+str(code)+'\n');queue.chmod(0o755)
        result=subprocess.run(['bash','-s','--','job'],input=REMOTE_ACTIVITY_SCRIPT,text=True,capture_output=True,
                              env={**os.environ,'HOME':str(root),'PATH':str(tools)+os.pathsep+os.environ['PATH']},check=True)
        return json.loads(result.stdout)

    def test_running_job_is_reported_without_opening_any_job_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            # No job directory exists: a scheduler-confirmed active job takes
            # the fast branch without probing metadata, locks or results.
            result=self.run_activity(root,str(root/'slurmy/jobs/job'))
            self.assertTrue(result['queue_available'])
            self.assertTrue(result['jobs'][0]['active'])
            self.assertTrue(result['jobs'][0]['exists'])
            self.assertEqual(result['jobs'][0]['controller_state'],'unknown')

    def test_idle_controller_and_stop_markers_are_checked_without_results(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/job';job.mkdir(parents=True)
            lock=job/'.submit-controller.lock';lock.touch()
            # This would hang a reader; the heartbeat must never open it.
            (job/'results').mkdir();os.mkfifo(job/'results/index.csv')
            self.assertEqual(self.run_activity(root)['jobs'][0]['controller_state'],'inactive')
            with lock.open() as stream:
                fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
                self.assertEqual(self.run_activity(root)['jobs'][0]['controller_state'],'active')
            (job/'.stopped').touch()
            self.assertTrue(self.run_activity(root)['jobs'][0]['stopped'])
            unavailable=self.run_activity(root,code=1)
            self.assertFalse(unavailable['queue_available'])
            self.assertIsNone(unavailable['jobs'][0]['exists'])

    def test_heartbeat_only_requests_fresh_verification_after_confirmed_idle(self):
        watch=RecoveryWatch();watch.resume('host','job')
        row=dict(job_id='job',active=False,controller_state='inactive',stopped=False,exists=True)
        value=dict(queue_available=True,jobs=[row])
        self.assertEqual(watch.idle_candidates('host',value,now=0),[])
        self.assertEqual(watch.idle_candidates('host',value,now=14),[])
        self.assertEqual(watch.idle_candidates('host',value,now=15),['job'])
        self.assertEqual(watch.entries[('host','job')]['attempts'],0)
        value['queue_available']=False
        self.assertEqual(watch.idle_candidates('host',value,now=16),[])
        value['queue_available']=True
        self.assertEqual(watch.idle_candidates('host',value,now=30),[])
        row['stopped']=True
        self.assertEqual(watch.idle_candidates('host',value,now=50),[])
        self.assertEqual(watch.targets('host'),[])

    def test_background_watch_does_not_collect_history_for_running_jobs(self):
        from slurmy_web import app as web
        row=dict(job_id='job',active=True,controller_state='unknown',stopped=False,exists=True)
        remote=SimpleNamespace(fetch_activity=Mock(return_value=dict(queue_available=True,jobs=[row])),fetch=Mock())
        with patch.object(web.workflows,'migrate_user_runs'):
            app=web.create_app();app.config['COLLECTOR']=lambda *args:remote
            watch=app.config['RECOVERY_WATCH'];watch.resume('host','job');watch.idle_seconds=0
            app.config['CHECK_IDLE_JOBS']()
            remote.fetch_activity.assert_called_once_with(['job']);remote.fetch.assert_not_called()
            row.update(active=False,controller_state='inactive')
            complete=JobSnapshot('job','/jobs/job',0,metadata={'task_count':1},completed=1,
                                 controller_state='inactive',queue_available=True)
            remote.fetch.return_value=ClusterSnapshot('host','/jobs','job',1,[complete])
            app.config['CHECK_IDLE_JOBS']()
            self.assertEqual(remote.fetch.call_count,1)
            self.assertEqual(remote.fetch.call_args.args[0],'job')
            self.assertEqual(watch.targets('host'),[])

    def test_activity_transport_is_bounded_and_rejects_bad_ids(self):
        response=dict(queue_available=True,jobs=[])
        with patch('slurmy_web.monitor.subprocess.run',return_value=SimpleNamespace(returncode=0,stdout=json.dumps(response).encode(),stderr=b'')) as run:
            self.assertEqual(RemoteCollector('host',10).fetch_activity([]),response)
            self.assertIn('timeout -k 5 15',run.call_args.args[0][-1])
            with self.assertRaises(ValueError):RemoteCollector('host',10).fetch_activity(['../bad'])
            self.assertEqual(run.call_count,1)
