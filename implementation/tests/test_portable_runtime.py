"""Portable process supervision: native Unix limits, cancellation and output."""
import _bootstrap  # noqa: F401
import io
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

try:
    import psutil
    import portable_runtime as runtime
except ImportError:
    psutil=None


@unittest.skipUnless(psutil and os.name=='posix','Portable supervision requires psutil and POSIX')
class PortableRuntimeTests(unittest.TestCase):
    def run_code(self,code,**options):
        out,err,combined=io.BytesIO(),io.BytesIO(),io.BytesIO()
        result=runtime.supervise([sys.executable,'-c',code],stdout=out,stderr=err,combined=combined,**options)
        return result,out.getvalue(),err.getvalue(),combined.getvalue()

    def test_native_options_preserve_literal_limits_and_reject_unknown_flags(self):
        options=runtime.native_options('runsolver --cpu-limit=2.5 -W 0 -R 64 --timestamp --add-eof --var "$VAR_FILE" bash solver.sh')
        self.assertEqual(options['cpu_limit'],2.5);self.assertEqual(options['wall_limit'],0);self.assertEqual(options['memory_mib'],64)
        self.assertTrue(options['timestamp']);self.assertTrue(options['add_eof'])
        self.assertEqual(runtime.native_options('runsolver bash solver.sh')['cpu_limit'],0)
        for flag in ['--unknown','--cpu-limit -1','--cpu-limit nan']:
            with self.subTest(flag=flag),self.assertRaises(ValueError):runtime.native_options('runsolver '+flag+' bash solver.sh')

    def test_streams_real_timestamps_eof_and_statistics(self):
        result,out,err,combined=self.run_code('import sys;print("hello");print("error",file=sys.stderr)',timestamp=True,add_eof=True)
        self.assertEqual(result['status'],'ok');self.assertEqual(out,b'hello\n');self.assertEqual(err,b'error\n')
        for line in combined.splitlines():self.assertRegex(line,rb'^\d+\.\d+/\d+\.\d+\t')
        self.assertTrue(combined.rstrip().endswith(b'EOF'));self.assertGreater(result['max_memory_kib'],0)
        self.assertGreaterEqual(result['cpu_seconds'],0);self.assertGreater(result['wall_seconds'],0)

    def test_wall_timeout_and_zero_disables_limits(self):
        result,*_=self.run_code('import time;time.sleep(10)',wall_limit=.1)
        self.assertTrue(result['timed_out']);self.assertLess(result['wall_seconds'],3)
        result,*_=self.run_code('import time;time.sleep(.15)',cpu_limit=0,wall_limit=0,memory_mib=0)
        self.assertEqual(result['status'],'ok')

    def test_cpu_limit(self):
        result,*_=self.run_code('while True: pass',cpu_limit=.1)
        self.assertEqual(result['status'],'time-limit');self.assertGreaterEqual(result['cpu_seconds'],.1)

    def test_memory_limit(self):
        result,*_=self.run_code('import time;x=bytearray(80*1024*1024);time.sleep(10)',memory_mib=48)
        self.assertEqual(result['status'],'memory-limit');self.assertTrue(result['memory_out'])
        self.assertGreater(result['max_memory_kib'],48*1024)

    def test_stop_kills_child_even_when_it_creates_a_new_session(self):
        with tempfile.TemporaryDirectory() as folder:
            pidfile=Path(folder)/'child'
            code='import subprocess,time; p=subprocess.Popen(['+repr(sys.executable)+',"-c","import time;time.sleep(30)"],start_new_session=True);open('+repr(str(pidfile))+',"w").write(str(p.pid));time.sleep(30)'
            start=time.monotonic()
            result,*_=self.run_code(code,cancel=lambda:pidfile.exists() and time.monotonic()-start>.15)
            self.assertEqual(result['status'],'interrupted')
            child=psutil.Process(int(pidfile.read_text())) if psutil.pid_exists(int(pidfile.read_text())) else None
            self.assertTrue(child is None or child.status()==psutil.STATUS_ZOMBIE)

    def test_darwin_topology_identity_memory_and_no_taskset(self):
        import local_backend as local
        with patch.object(local.platform,'system',return_value='Darwin'):
            self.assertGreater(len(local.cpu_groups()),0);self.assertGreater(local.memory_mib(),0)
            identity=local.process_identity(os.getpid());self.assertTrue(identity)
            self.assertTrue(local.alive(dict(pid=os.getpid(),identity=identity)))
            self.assertFalse(local.alive(dict(pid=os.getpid(),identity='different process')))
            self.assertEqual(local.affinity_command('0',['python','program.py']),['python','program.py'])

    def test_bundled_preparation_uses_portable_limiter_and_skips_linux_build(self):
        import local_backend as local
        sys.path.insert(0,str(local.REPO/'web-app'))
        from slurmy_web import workflows
        with tempfile.TemporaryDirectory() as folder,patch.dict(os.environ,SLURMY_LOCAL_ROOT=folder+'/state'):
            directory=Path(folder)/'workflow';directory.mkdir();solver=Path(folder)/'solver';solver.mkdir()
            executable=solver/'solver';executable.write_text('#!/bin/sh\necho "% SZS status Theorem"\n');executable.chmod(0o755)
            problem=Path(folder)/'SET+1.p';problem.write_text('cnf(a,axiom,p).')
            row=dict(solver_name='Example',solver_directory=str(solver),command='./solver {{problem}}',problem=str(problem),cores=1,threads_per_core=1,cpu_limit=1,wc_limit=1,mem_limit='64MiB',cpus='auto',exclusive_cpu='false',exclusive_node='false')
            (directory/'solver-problem-pairs.csv').write_text(workflows.csv_text([row]))
            bundled=local.REPO/'implementation/build/runsolver'
            (directory/'building.json').write_text(json.dumps([dict(root=str(solver),recipe=''),dict(root=str(bundled),recipe=str(bundled/'build.sh'),artifact='runsolver')]))
            (directory/'resource-limiter.txt').write_text(str(bundled/'runsolver')+' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --solver-data {{solver_log}} --timestamp --add-eof {{solver_command}}')
            (directory/'Makefile').write_text('BATCH_SIZE := 1\nGET_COMPUTER_INFO := false\n')
            with patch.object(local.platform,'system',return_value='Darwin'):local.prepare(directory)
            metadata=local.read_json(directory/'submit.sh.files/metadata.json')
            self.assertTrue(metadata['portable_limiter']);self.assertEqual(metadata['execution_platform'],'Darwin')
            self.assertFalse(any(build.get('recipe') for build in local.read_json(directory/'submit.sh.files/local-builds.json',[])))
            # Execute the Mac pair path on this machine as a portability regression.
            job=Path(folder)/'job';__import__('shutil').copytree(directory/'submit.sh.files',job)
            (job/'rootfs'/str(solver).lstrip('/')).parent.mkdir(parents=True,exist_ok=True)
            __import__('shutil').copytree(solver,job/'rootfs'/str(solver).lstrip('/'))
            __import__('shutil').copy2(problem,job/'rootfs'/str(problem).lstrip('/'))
            for sub in ['progress','results/index']:(job/sub).mkdir(parents=True,exist_ok=True)
            row=json.loads((job/'manifest.jsonl').read_text().strip())
            self.assertTrue(runtime.run_pair(job,row,0))
            results=local.results(job);self.assertEqual(results[0]['szs_status'],'Theorem')
            with __import__('tarfile').open(job/'results'/results[0]['archive']) as archive:
                combined=next(member for member in archive if member.name.endswith('.solver-combined.log'))
                self.assertTrue(archive.extractfile(combined).read().rstrip().endswith(b'EOF'))
