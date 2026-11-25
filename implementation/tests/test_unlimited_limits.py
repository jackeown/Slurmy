"""Zero means unlimited only for limits, including generated commands and reports."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from slurmy import COLUMNS, SlurmyError, generate, read_builds, omit_unlimited_arguments, memory_bytes, nonnegative, read_pairs


class UnlimitedLimitTests(unittest.TestCase):
    def test_zero_limits_survive_generation_and_core_zero_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'p.p').touch();(root/'limiter').touch()
            row=dict(solver_name='Test',command='./solver {{cpu_limit}} {{wc_limit}} {{mem_limit_mib}} {{problem}}',
                     solver_directory=str(root),problem=str(root/'p.p'),cpu_limit=0,wc_limit=0,
                     mem_limit='0',cores=1,threads_per_core=1,exclusive_cpu='false',exclusive_node='false',cpus='auto')
            pairs=root/'jobpairs.csv'
            def write():
                with pairs.open('w') as stream:
                    w=csv.DictWriter(stream,fieldnames=COLUMNS);w.writeheader();w.writerow(row)
            write()
            self.assertEqual([read_pairs(pairs)[0][key] for key in ('cpu_limit','wc_limit','mem_limit')],[0,0,0])
            (root/'building.txt').write_text(json.dumps([dict(root=str(root))]))
            (root/'limiter.txt').write_text(str(root/'limiter')+' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} {{solver_command}}')
            generate(pairs,root/'building.txt',root/'limiter.txt')
            assets=root/'submit.sh.files'
            plan=(assets/'plans/batch_000000.sh').read_text()
            self.assertIn('MEMORY_MIB=0\n',plan);self.assertIn('WALL_SECONDS=604800\n',plan)
            limiter=(assets/'calls/0/limiter.sh').read_text()
            self.assertIn('--cpu-limit 0 --wall-clock-limit 0 --rss-swap-limit 0',limiter)
            self.assertIn('./solver 0 0 0',(assets/'calls/0/solver.sh').read_text())
            for field in ('cores','threads_per_core'):
                row[field]=0;write()
                with self.assertRaises(SlurmyError):read_pairs(pairs)
                row[field]=1
            for field in ('cpu_limit','wc_limit','mem_limit'):
                row[field]='-1';write()
                with self.assertRaises(SlurmyError):read_pairs(pairs)
                row[field]=0

    def test_omit_solver_arguments_preserves_shell_text_and_finite_limits(self):
        import shlex
        task=dict(cpu_limit=0,wc_limit=0,mem_limit=0,omit_cpu_limit=True,omit_wc_limit=True,omit_mem_limit=True)
        for option in ('--cpu-limit {{cpu_limit}}','--cpu-limit={{cpu_limit}}',"'-timeout({{cpu_limit}})'",'-t{{cpu_limit}}'):
            result=omit_unlimited_arguments("'./my solver' "+option+" --other 'keep spaces' {{problem}} > output.log",task)
            self.assertEqual(shlex.split(result),['./my solver','--other','keep spaces','{{problem}}','>','output.log'])
            self.assertIn("'keep spaces'",result)
        command='./solver --cpu-limit {{cpu_limit}} --wall={{wc_limit}} --mem={{mem_limit_mib}} --memory {{mem_limit}} {{problem}}'
        self.assertEqual(shlex.split(omit_unlimited_arguments(command,task)),['./solver','{{problem}}'])
        task['cpu_limit']=2
        self.assertIn('--cpu-limit {{cpu_limit}}',omit_unlimited_arguments(command,task))
        task['cpu_limit']=0;task['omit_cpu_limit']=False
        self.assertIn('--cpu-limit {{cpu_limit}}',omit_unlimited_arguments(command,task))
        task['omit_cpu_limit']=True
        for bad in ('./solver --limits={{cpu_limit}},{{cores}}','./solver "$(echo {{wc_limit}})"','./solver {{wc_limit}}'):
            with self.assertRaises(SlurmyError):omit_unlimited_arguments(bad,task)

    def test_generated_prover_omission_does_not_change_runsolver_zero_arguments(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'p.p').touch();(root/'limiter').touch()
            row=dict(solver_name='Test',command='./solver --cpu-limit {{cpu_limit}} --wall={{wc_limit}} --mem={{mem_limit_mib}} {{problem}}',
                     solver_directory=str(root),problem=str(root/'p.p'),cpu_limit=0,wc_limit=0,mem_limit=0,
                     omit_cpu_limit='true',omit_wc_limit='true',omit_mem_limit='true',cores=1,threads_per_core=1)
            with (root/'pairs.csv').open('w') as stream:
                w=csv.DictWriter(stream,fieldnames=COLUMNS);w.writeheader();w.writerow(row)
            (root/'builds.txt').write_text(json.dumps([dict(root=str(root))]))
            (root/'limiter.txt').write_text(str(root/'limiter')+' --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} {{solver_command}}')
            generate(root/'pairs.csv',root/'builds.txt',root/'limiter.txt')
            assets=root/'submit.sh.files/calls/0'
            solver=(assets/'solver.sh').read_text()
            self.assertNotIn('--cpu-limit',solver);self.assertNotIn('--wall',solver);self.assertNotIn('--mem',solver)
            self.assertIn('--cpu-limit 0 --wall-clock-limit 0 --rss-swap-limit 0',(assets/'limiter.sh').read_text())

    def test_existing_default_builds_automatically_include_bundled_source(self):
        from slurmy import REPO
        with tempfile.TemporaryDirectory() as folder:
            definition=Path(folder)/'building.txt'
            definition.write_text(json.dumps([dict(root=str(REPO/'implementation/build/runsolver'),
                recipe=str(REPO/'implementation/build/runsolver/build.sh'),source='',artifact='runsolver')]))
            self.assertEqual(read_builds(definition)[0].source,REPO/'implementation/build/runsolver/source')

    def test_zero_units_and_invalid_limits(self):
        for value in ('0','0MiB','0MB','0GiB'):
            self.assertEqual(memory_bytes(value),0)
        for value in ('-1','1.5','', 'all'):
            with self.assertRaises(SlurmyError):nonnegative(value,'CPU time')
            with self.assertRaises(SlurmyError):memory_bytes(value)
        self.assertEqual(nonnegative('0','wall time'),0)
        self.assertEqual(memory_bytes('2GiB'),2**31)

    def test_real_updated_computer_report_uses_comments_and_optional_timestamps(self):
        program=Path(__file__).parent.parent/'tools/GetComputerInfo'
        for timestamp in (False,True):
            result=subprocess.run(['bash',str(program),'-p','--cpu-limit','0','--wc-limit','0',
                '--mem-limit','0MiB',*(['--timestamp'] if timestamp else []),
                'THIS','CPULimit','WCLimit','MemLimit'],capture_output=True,text=True,check=True,timeout=45)
            lines=result.stdout.splitlines()
            self.assertEqual(len(lines),4)
            for line in lines:
                self.assertTrue(line.startswith('0.00/0.00\t% ' if timestamp else '% '),line)
            self.assertIn('CPULimit : 0',result.stdout)
            self.assertIn('WCLimit  : 0',result.stdout)
            self.assertIn('MemLimit : 0MiB',result.stdout)
