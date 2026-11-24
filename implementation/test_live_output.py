"""Exercise generated workers with the real runsolver, including stream timing."""
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import zipfile

from slurmy import COLUMNS, generate
from analyze_run import prepared_scripts_current
from remote_export import export as compact_export
from starexec_export import export as starexec_export


class LiveOutputTests(unittest.TestCase):
    def run_capture(self, command, wall=5, computer_info=False, environment="", analysis=None, extra_analyses=()):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        runtime = root/'runtime'; runtime.mkdir()
        binary = Path(__file__).parent/'build/runsolver/runsolver'
        shutil.copy2(binary, runtime/'runsolver')
        problem = root/'example.p'; problem.touch()
        row = dict(solver_name='test',command=command,solver_directory=str(runtime),
                   problem=str(problem),wc_limit=wall,cpu_limit=0,mem_limit='100MiB',
                   cores=1,threads_per_core=1,cpus='auto',exclusive_cpu='false',exclusive_node='false',environment=environment)
        with (root/'jobpairs.csv').open('w',newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=COLUMNS)
            writer.writeheader()
            for index, script in enumerate((analysis, *extra_analyses)):
                fields = ''
                if script:
                    name, source, *columns = script
                    (root/name).write_text(source)
                    fields = columns[0] if columns else ''
                writer.writerow({**row, 'solver_name': f'solver-{index}',
                                 'analysis_script': script[0] if script else '', 'analysis_columns': fields})
        (root/'building.txt').write_text(str(runtime)+'\n\n')
        (root/'limiter.txt').write_text(str(runtime/'runsolver')+
            ' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}}'
            ' --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}}'
            ' --timestamp --add-eof {{solver_command}}')
        generate(root/'jobpairs.csv',root/'building.txt',root/'limiter.txt',get_computer_info=computer_info,
                 batch_size=1 + len(extra_analyses))
        job=root/'submit.sh.files'
        (job/'GetComputerInfo').write_text("printf '0.00/0.00\\t%% Computer : test-node\\n'\n")
        mirrored=job/'rootfs'/str(runtime).lstrip('/');mirrored.mkdir(parents=True)
        shutil.copy2(binary,mirrored/'runsolver')
        for folder in ('results/index','progress'): (job/folder).mkdir(parents=True,exist_ok=True)
        for index in range(1 + len(extra_analyses)):
            result = subprocess.run(['bash', str(job/'call.sh'), str(index), '0'],
                                    env={**os.environ, 'JOB_DIR': str(job)}, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr)
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

    def test_analyzer_reads_run_logs_and_extends_the_standard_export(self):
        python = ('metrics.py', 'import json, sys\nfrom pathlib import Path\n'
                  'out, err = [Path(path).read_text() for path in sys.argv[1:]]\n'
                  'print(json.dumps({"stdout_bytes": len(out), "stderr": err, "status": "custom"}))\n',
                  'stderr, stdout_bytes, status, unreported')
        shell = ('other metrics.sh', 'echo \'{"decoy": 99}\' >&2\nprintf "stderr_bytes\\n%s\\n" "$(wc -c < "$2")"\n',
                 'stderr_bytes')
        job, row, logs = self.run_capture('printf answer; printf warning >&2', computer_info=True,
                                          analysis=python, extra_analyses=(shell, None))
        self.assertEqual(json.loads(row['analysis'])['stdout_bytes'], 6)
        self.assertEqual(logs['solver'], b'answer')
        self.assertEqual(logs['solver-stderr'], b'warning')
        self.assertIn('WALL_SECONDS=445\n', (job/'plans/batch_000000.sh').read_text())
        compact_export(job)
        summary = next((job/'exports').glob('*_results-summary.csv'))
        with summary.open(newline='') as stream:
            exported = list(csv.DictReader(stream))
        self.assertEqual([item['stdout_bytes'] for item in exported], ['6', '', ''])
        self.assertEqual([item['stderr_bytes'] for item in exported], ['', '7', ''])
        self.assertEqual([item['unreported'] for item in exported], ['', '', ''])
        self.assertEqual(list(exported[0])[-5:], ['stderr', 'stdout_bytes', 'analysis.status', 'unreported', 'stderr_bytes'])
        self.assertEqual(exported[0]['stderr'], 'warning')
        self.assertEqual(exported[0]['analysis.status'], 'custom')
        self.assertEqual(exported[0]['status'], row['status'])
        self.assertTrue(prepared_scripts_current(job))
        (job.parent/shell[0]).write_text('echo changed')
        self.assertFalse(prepared_scripts_current(job))

    def test_failed_analyzer_preserves_the_solver_result_and_logs(self):
        _, row, logs = self.run_capture('printf answer', analysis=('analysis.sh', 'printf "unexpected\\n9\\n"', 'metric'))
        self.assertEqual(row['complete'], 'true')
        self.assertEqual(row['return_code'], '0')
        self.assertEqual(row['analysis'], '')
        self.assertEqual(logs['solver'], b'answer')
        self.assertIn(b'Unexpected analysis columns: unexpected', logs['controller'])


if __name__ == '__main__': unittest.main()
