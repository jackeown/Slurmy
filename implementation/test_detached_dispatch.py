"""Submission outlives its SSH launcher and duplicate launches are harmless."""
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

TEMPLATES = Path(__file__).parent / 'templates'


class DetachedDispatchTests(unittest.TestCase):
    def test_default_launcher_keeps_session_open_and_releases_launch_lock(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)/'slurmy/jobs/test-job'; job.mkdir(parents=True)
            (job/'remote_submit.sh').write_text('exec 9> .submit-controller.lock; flock -n 9; touch started; sleep 1; touch survived\n')
            command = ['bash', str(TEMPLATES/'remote_dispatch.sh'), 'test-job']
            env = {**os.environ, 'SLURMY_CONTROLLER_MODE': 'connected', 'HOME': folder}
            env.pop('SLURMY_KEEP_SUBMISSION_SESSION', None)
            first = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic()+3
                while not (job/'started').exists() and time.monotonic()<deadline: time.sleep(.02)
                self.assertIsNone(first.poll())
                duplicate = subprocess.run(command, env=env, capture_output=True, text=True, timeout=2)
                self.assertIn('already active', duplicate.stdout)
                output, error = first.communicate(timeout=3)
                self.assertEqual(first.returncode, 0, error)
                self.assertIn('Keeping SSH connected', output)
                self.assertTrue((job/'survived').is_file())
            finally:
                if first.poll() is None: first.kill(); first.communicate()

    def test_failure_after_launch_propagates_to_the_connected_client(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder)/'slurmy/jobs/test-job'; job.mkdir(parents=True)
            (job/'remote_submit.sh').write_text('sleep .3; echo axiom-failure >&2; exit 17\n')
            result = subprocess.run(['bash', str(TEMPLATES/'remote_dispatch.sh'), 'test-job'],
                                    env={**os.environ, 'SLURMY_CONTROLLER_MODE': 'connected', 'HOME': folder}, capture_output=True, text=True, timeout=3)
            self.assertEqual(result.returncode, 17)
            self.assertIn('axiom-failure', result.stderr)

    def test_controller_outlives_launcher_and_second_launch_keeps_existing_controller(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder) / 'slurmy/jobs/test-job'
            job.mkdir(parents=True)
            (job / 'remote_submit.sh').write_text('''#!/bin/bash
set -eu
exec 9> .submit-controller.lock
flock -n 9
printf 'started\\n' >> starts
printf 'submitting 0 1\\n' > submission.state
sleep 1
printf 'complete 1 1\\n' > submission.state
touch survived
''')
            env = {**os.environ, 'SLURMY_CONTROLLER_MODE': 'connected', 'HOME': folder, 'SLURMY_KEEP_SUBMISSION_SESSION': '0'}
            command = ['bash', str(TEMPLATES / 'remote_dispatch.sh'), 'test-job']
            first = subprocess.run(command, env=env, capture_output=True, text=True, timeout=3)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertIn('Slurmy job ID: test-job', first.stdout)
            self.assertFalse((job / 'survived').exists())
            second = subprocess.run(command, env=env, capture_output=True, text=True, timeout=3)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertIn('already active', second.stdout)
            deadline = time.monotonic() + 3
            while not (job / 'survived').exists() and time.monotonic() < deadline:
                time.sleep(.05)
            self.assertTrue((job / 'survived').exists())
            self.assertEqual((job / 'starts').read_text(), 'started\n')

    def test_controller_survives_termination_of_launcher_process_group(self):
        import signal
        import shlex
        with tempfile.TemporaryDirectory() as folder:
            job=Path(folder)/'slurmy/jobs/test-job';job.mkdir(parents=True)
            (job/'remote_submit.sh').write_text('exec 9> .submit-controller.lock; flock -n 9; touch started; sleep 1; touch survived\n')
            command='bash '+shlex.quote(str(TEMPLATES/'remote_dispatch.sh'))+' test-job; touch '+shlex.quote(str(job/'launched'))+'; sleep 30'
            parent=subprocess.Popen(['bash','-c',command],env={**os.environ,'SLURMY_CONTROLLER_MODE':'connected','HOME':folder,'SLURMY_KEEP_SUBMISSION_SESSION':'0'},start_new_session=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            try:
                deadline=time.monotonic()+3
                while not (job/'launched').exists() and time.monotonic()<deadline:time.sleep(.02)
                self.assertTrue((job/'launched').exists())
                os.killpg(parent.pid,signal.SIGTERM);parent.wait(timeout=3)
                deadline=time.monotonic()+3
                while not (job/'survived').exists() and time.monotonic()<deadline:time.sleep(.02)
                self.assertTrue((job/'survived').exists())
            finally:
                if parent.poll() is None:os.killpg(parent.pid,signal.SIGKILL);parent.wait()

    def test_early_failure_reports_controller_log(self):
        with tempfile.TemporaryDirectory() as folder:
            job = Path(folder) / 'slurmy/jobs/test-job'
            job.mkdir(parents=True)
            (job / 'remote_submit.sh').write_text('echo "Invalid topology" >&2; exit 23\n')
            result = subprocess.run(['bash', str(TEMPLATES / 'remote_dispatch.sh'), 'test-job'],
                                    env={**os.environ, 'SLURMY_CONTROLLER_MODE': 'connected', 'HOME': folder}, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Invalid topology', result.stderr)
            self.assertTrue((job/'.submit-controller.lock').exists())
            self.assertTrue((job/'submission.state').read_text().startswith('failed '))

    def test_legacy_resume_embeds_current_dispatcher(self):
        # Existing cluster jobs must receive exactly the same robust launcher.
        resume = (TEMPLATES / 'remote_resume.sh').read_text()
        embedded = resume.split("<<'SLURMY_DISPATCH'\n", 1)[1].split('SLURMY_DISPATCH\n', 1)[0]
        self.assertEqual(embedded, (TEMPLATES / 'remote_dispatch.sh').read_text())

    def test_resume_uses_compact_results_and_retries_only_unfinished_pairs(self):
        import json
        import sys
        script = (TEMPLATES / 'remote_submit.sh').read_text()
        code = script.split("resume_batches=$(python3 - <<'PY'\n", 1)[1].split('\nPY\n', 1)[0]
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'metadata.json').write_text(json.dumps({'planned_batch_tasks': [[0], [1], [2], [3]]}))
            index = root / 'results/index'
            index.mkdir(parents=True)
            (index / '.ready').touch()
            (index / 'batch_000000.csv').write_text('task_id,complete\n0,true\n1,false\n2,true\n')
            # A raw directory scan would incorrectly count task 3 as finished.
            (root / 'results/batch_000003_task_000000003.csv').write_text('task_id,complete\n3,true\n')
            result = subprocess.run([sys.executable, '-c', code], cwd=root, capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.splitlines(), ['1', '3'])
            (index / '.ready').unlink()
            result = subprocess.run([sys.executable, '-c', code], cwd=root, capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.splitlines(), ['0', '1', '2'])

    def test_automatic_recovery_never_stops_live_or_intentionally_stopped_work(self):
        for state in ('running', 'stopped', 'queue-error'):
            with self.subTest(state=state), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                job = root / 'slurmy/jobs/test-job'
                job.mkdir(parents=True)
                if state == 'stopped':
                    (job / '.stopped').touch()
                tools = root / 'tools'
                tools.mkdir()
                queue = tools / 'squeue'
                queue.write_text('#!/bin/sh\n' + ('exit 23\n' if state == 'queue-error' else f'echo "{job}"\n'))
                queue.chmod(0o755)
                script = (TEMPLATES / 'remote_recover.sh').read_text() + '\ntouch "$job/SHOULD_NOT_RUN"\n'
                result = subprocess.run(['bash', '-s', '--', 'test-job'], input=script, text=True,
                                        env={**os.environ, 'SLURMY_CONTROLLER_MODE': 'connected', 'HOME': folder, 'PATH': str(tools) + os.pathsep + os.environ['PATH']},
                                        capture_output=True)
                self.assertEqual(result.returncode == 0, state != 'queue-error')
                self.assertFalse((job / 'SHOULD_NOT_RUN').exists())
                self.assertEqual((job / '.stopped').exists(), state == 'stopped')

    def test_controller_does_not_rerun_noninteractive_shell_startup_hooks(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);job=root/'slurmy/jobs/test-job';job.mkdir(parents=True)
            hook=root/'shell-hook.sh'
            hook.write_text('if [[ "$0" == *remote_submit.sh ]]; then echo "bind: line editing not enabled" >&2; exit 23; fi\n')
            (job/'remote_submit.sh').write_text('printf "%s" "$CLUSTER_SETTING" > preserved; touch started\n')
            result=subprocess.run(['bash',str(TEMPLATES/'remote_dispatch.sh'),'test-job'],
                                  env={**os.environ,'SLURMY_CONTROLLER_MODE':'connected','HOME':folder,'BASH_ENV':str(hook),'CLUSTER_SETTING':'preserved value'},capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertTrue((job/'started').exists())
            self.assertEqual((job/'preserved').read_text(),'preserved value')
            self.assertNotIn('bind:',(job/'submission-controller.log').read_text())
