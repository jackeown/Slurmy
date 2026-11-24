"""Exercise generated workers with the real runsolver, including stream timing."""
import csv
import os
from pathlib import Path
import shutil
import json
import signal
import time
import subprocess
import tarfile
import tempfile
import unittest
import zipfile

from slurmy import COLUMNS, generate
from remote_export import export as compact_export
from starexec_export import export as starexec_export


class LiveOutputTests(unittest.TestCase):
    def run_capture(self, command, wall=5, computer_info=False, environment="", problem_name="example.p", postprocessor_code=None, postprocessor_mode="default", postprocessor_timeout=30, cancel_post=False, postprocessor_command=None, native_capture=True):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        runtime = root/'runtime'; runtime.mkdir()
        binary = Path(__file__).parent/'build/runsolver/runsolver'
        shutil.copy2(binary, runtime/'runsolver')
        problem = root/problem_name; problem.touch()
        row = dict(solver_name='test',command=command,solver_directory=str(runtime),
                   problem=str(problem),wc_limit=wall,cpu_limit=0,mem_limit='100MiB',
                   cores=1,threads_per_core=1,cpus='auto',exclusive_cpu='false',exclusive_node='false',environment=environment)
        row.update(postprocessor_mode=postprocessor_mode,postprocessor_timeout=postprocessor_timeout)
        postprocessor=root/'postprocessor tools'
        if postprocessor_code is not None:
            postprocessor.mkdir();(postprocessor/'post.py').write_text(postprocessor_code)
            row.update(postprocessor_mode='custom',postprocessor_directory=str(postprocessor),postprocessor_command=postprocessor_command or '/usr/bin/python3 ./post.py --label=example')
        with (root/'jobpairs.csv').open('w',newline='') as stream:
            writer=csv.DictWriter(stream,fieldnames=COLUMNS);writer.writeheader();writer.writerow(row)
        (root/'building.txt').write_text(str(runtime)+'\n\n')
        (root/'limiter.txt').write_text(str(runtime/'runsolver')+
            ' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}}'
            ' --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}}'
            ' --timestamp --add-eof {{solver_command}}')
        if not native_capture:
            limiter=root/'limiter.txt';limiter.write_text(limiter.read_text().replace(' --solver-data {{solver_log}}',''))
        generate(root/'jobpairs.csv',root/'building.txt',root/'limiter.txt',get_computer_info=computer_info)
        job=root/'submit.sh.files'
        (job/'GetComputerInfo').write_text("printf '0.00/0.00\\t%% Computer : test-node\\n'\n")
        mirrored=job/'rootfs'/str(runtime).lstrip('/');mirrored.mkdir(parents=True)
        shutil.copy2(binary,mirrored/'runsolver')
        if postprocessor.is_dir():
            paths=(job/'archive-paths.txt').read_bytes().decode().split('\x00')
            self.assertIn(str(postprocessor).lstrip('/'),paths)
            shutil.copytree(postprocessor,job/'rootfs'/str(postprocessor).lstrip('/'))
        for folder in ('results/index','progress'): (job/folder).mkdir(parents=True,exist_ok=True)
        arguments=['bash',str(job/'call.sh'),'0','0'];environment={**os.environ,'JOB_DIR':str(job)}
        if cancel_post:
            process=subprocess.Popen(arguments,env=environment,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                for _ in range(100):
                    progress=job/'progress/task_0.csv'
                    if progress.exists() and next(csv.reader([progress.read_text()]))[0]=='postprocessing':break
                    if process.poll() is not None:break
                    time.sleep(.05)
                process.send_signal(signal.SIGTERM)
                stdout,stderr=process.communicate(timeout=10)
                result=subprocess.CompletedProcess(arguments,process.returncode,stdout,stderr)
            finally:
                if process.poll() is None:process.kill();process.communicate()
        else:result=subprocess.run(arguments,env=environment,capture_output=True,text=True,timeout=15)
        self.assertEqual(result.returncode,0,result.stderr)
        with (job/'results/batch_000000_task_000000000.csv').open(newline='') as stream:
            row=next(csv.DictReader(stream))
        with tarfile.open(job/'results'/row['archive']) as archive:
            logs={member.name.split('.')[-2]:archive.extractfile(member).read()
                  for member in archive if member.isfile() and member.name.endswith('.log')}
        return job,row,logs

    def test_solver_receives_literal_environment_without_leaking_to_parent(self):
        command="/bin/bash -c 'printf \"%s\\n\" \"$SLURMY_TEST_ENV\" \"$Hello\"'"
        _,_,logs=self.run_capture(command,environment='SLURMY_TEST_ENV="$(echo unsafe); $HOME" Hello = World')
        self.assertEqual(logs['solver'],b'$(echo unsafe); $HOME\nWorld\n')
        self.assertNotIn('SLURMY_TEST_ENV',os.environ)

    def test_environment_placeholders_resolve_limits_and_safe_cluster_problem_path(self):
        command="/bin/bash -c 'printf \"%s\\n\" \"$THREADS\" \"$CPU\" \"$WALL\" \"$MEM\" \"$INPUT\" \"$TEXT\"'"
        name="problem with spaces'$(echo BAD)+1.p"
        job,_,logs=self.run_capture(command,environment='THREADS={{cores}} CPU={{cpu_limit}} WALL={{wc_limit}} MEM={{mem_limit_mib}} INPUT={{problem}} TEXT="pre {{problem}} post"',problem_name=name)
        problem=job/'rootfs'/str(job.parent/name).lstrip('/')
        self.assertEqual(logs['solver'].decode().splitlines(),['1','0','5','100',str(problem),'pre '+str(problem)+' post'])

    def test_custom_postprocessor_receives_two_streams_and_controls_status(self):
        script="""import pathlib,sys
assert sys.argv[1]=='--label=example'
stdout,stderr=map(pathlib.Path,sys.argv[-2:])
assert 'SZS status Theorem' in stdout.read_text()
assert stderr.read_text()=='diagnostic'
print('custom report')
print('SZS data status=sat')
print('SZS data score = 42')
print('SZS data note = a, "b" = c')
print('postprocessor diagnostic',file=sys.stderr)
"""
        _,row,logs=self.run_capture("/bin/bash -c 'printf \"%% SZS status Theorem\\n\"; printf diagnostic >&2'",postprocessor_code=script)
        self.assertEqual(row['postprocess_status'],'sat')
        self.assertEqual(json.loads(row['postprocess_data']),{'status':'sat','score':'42','note':'a, "b" = c'})
        self.assertEqual(row['postprocess_return_code'],'0');self.assertEqual(row['postprocess_error'],'')
        self.assertIn(b'custom report',logs['postprocessor']);self.assertIn(b'postprocessor diagnostic',logs['postprocessor-stderr'])

    def test_streams_are_captured_without_limiter_solver_data_option(self):
        _,row,logs=self.run_capture("printf '%% SZS status Theorem\\n'",native_capture=False)
        self.assertEqual(row['szs_status'],'Theorem');self.assertEqual(row['postprocess_return_code'],'0')
        self.assertEqual(logs['solver'],b'% SZS status Theorem\n')

    def test_postprocessor_entrypoint_supports_environment_prefix(self):
        script="import os;assert os.environ['POST_MODE']=='test';print('SZS data status=custom')"
        _,row,_=self.run_capture('printf solver-output',postprocessor_code=script,postprocessor_command='POST_MODE=test python3 ./post.py')
        self.assertEqual(row['postprocess_status'],'custom')

    def test_disabled_postprocessing_does_not_recognize_solver_status(self):
        _,row,logs=self.run_capture("printf '%% SZS status Theorem\\n'",postprocessor_mode='none')
        self.assertEqual(row['szs_status'],'');self.assertEqual(row['postprocess_data'],'{}')
        self.assertNotIn('postprocessor',logs)

    def test_postprocessor_failure_timeout_and_cancellation_keep_solver_results(self):
        for script,timeout,cancel,expected in [
            ('raise SystemExit(7)',30,False,'exited with status 7'),
            ('import time;time.sleep(5)',1,False,'timed out'),
            ('import time;time.sleep(10)',30,True,'interrupted')]:
            with self.subTest(expected=expected):
                _,row,logs=self.run_capture("printf solver-output",postprocessor_code=script,postprocessor_timeout=timeout,cancel_post=cancel)
                self.assertEqual(row['complete'],'true');self.assertEqual(row['return_code'],'0')
                self.assertIn(expected,row['postprocess_error'])
                self.assertEqual(logs['solver'],b'solver-output')

    def test_early_stderr_is_measured_live_and_exported_once(self):
        job,row,logs=self.run_capture("/bin/bash -c 'printf \"early error\\n\" >&2; sleep 0.8; printf \"late stdout\\n\"; exit 7'",computer_info=True)
        self.assertEqual(row['return_code'],'7')
        header=b'0.00/0.00\t% Computer : test-node\n'
        self.assertEqual(logs['solver'],b'late stdout\n')
        self.assertEqual(logs['solver-stderr'],b'early error\n')
        combined=logs['solver-combined']
        self.assertTrue(combined.startswith(header))
        lines=combined.removeprefix(header).splitlines()
        self.assertIn(b'early error',lines[0]);self.assertIn(b'late stdout',lines[1])
        first_wall=float(lines[0].split()[0].split(b'/')[1])
        last_wall=float(lines[1].split()[0].split(b'/')[1])
        self.assertLess(first_wall,0.5);self.assertGreaterEqual(last_wall,0.7)
        self.assertTrue(lines[-1].endswith(b'\tEOF'))
        compact_export(job,combined=True)
        names,*_=starexec_export(job/'exports/starexec',job/'download','test')
        with zipfile.ZipFile(job/'download'/names[1]) as archive:
            text=archive.read(archive.namelist()[0])
            self.assertEqual(text,combined)
            self.assertEqual(text.count(b'early error'),1)

    def test_timeout_drains_streams_and_preserves_partial_output(self):
        _,row,logs=self.run_capture("/bin/bash -c 'printf partial; printf diagnostic >&2; sleep 10'",wall=1)
        self.assertEqual(row['status'],'time-limit')
        self.assertEqual(logs['solver'],b'partial')
        self.assertEqual(logs['solver-stderr'],b'diagnostic')
        self.assertIn(b'partial',logs['solver-combined'])
        self.assertIn(b'diagnostic',logs['solver-combined'])
        self.assertTrue(logs['solver-combined'].endswith(b'EOF\n'))


if __name__ == '__main__': unittest.main()
