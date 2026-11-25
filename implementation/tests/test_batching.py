"""Sequential batching preserves exclusivity without splitting pairs."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import base64
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from slurmy import COLUMNS, REPO, generate


class SequentialBatchTests(unittest.TestCase):
    def prepare(self, root, flags, size, pair_order=None, core_counts=None, limits=(6, 3, '100MiB'), overrides=None):
        problem = root/'example.p'
        problem.touch()
        (root/'limiter').touch()
        rows = []
        for i, (socket, node) in enumerate(flags):
            rows.append(dict(solver_name=f'Solver_{i%2}', command='./solver {{problem}}',
                solver_directory=str(root), problem=str(problem), wc_limit=limits[0], cpu_limit=limits[1],
                mem_limit=limits[2], cores=core_counts[i] if core_counts else 2, threads_per_core=1,
                cpus='auto', exclusive_cpu=str(socket).lower(), exclusive_node=str(node).lower()))
            if overrides:
                rows[-1].update(overrides[i])
        pairs=root/'jobpairs.csv'
        with pairs.open('w', newline='') as output:
            writer=csv.DictWriter(output, fieldnames=COLUMNS)
            writer.writeheader(); writer.writerows(rows)
        (root/'building.txt').write_text(json.dumps([{'root':str(root)}]))
        (root/'limiter.txt').write_text(f'{root}/limiter {{{{solver_command}}}}')
        generate(pairs,root/'building.txt',root/'limiter.txt',batch_size=size,pair_order=pair_order)
        return root/'submit.sh.files'

    def test_dedicated_requests_still_pack_100_pairs_per_batch(self):
        for flags in [(False,False), (True,False), (False,True), (True,True)]:
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as folder:
                assets=self.prepare(Path(folder),[flags]*201,100)
                metadata=json.loads((assets/'metadata.json').read_text())
                self.assertEqual([len(ids) for ids in metadata['planned_batch_tasks']],[100,100,1])
                self.assertEqual(metadata['planned_batch_tasks'][0],list(range(100)))

    def test_interleaved_sharing_modes_are_grouped_without_spreading_exclusivity(self):
        for order in ('solver-major', 'problem-major', 'random', None):
            with self.subTest(order=order), tempfile.TemporaryDirectory() as folder:
                assets = self.prepare(Path(folder), [(False,False), (True,False), (False,True)]*2,
                                      100, pair_order=order)
                metadata=json.loads((assets/'metadata.json').read_text())
                self.assertEqual(metadata['planned_batch_tasks'], [[0,3],[1,4],[2,5]])
                for index, (socket,node) in enumerate([(False,False),(True,False),(False,True)]):
                    plan=(assets/'plans'/f'batch_{index:06d}.sh').read_text()
                    self.assertIn(f'EXCLUSIVE_CPU={int(socket)}',plan)
                    self.assertIn(f'EXCLUSIVE_NODE={int(node)}',plan)

    def test_email_shared_8_core_and_dedicated_12_core_requests_have_separate_plans(self):
        with tempfile.TemporaryDirectory() as folder:
            assets=self.prepare(Path(folder),[(False,False),(True,False)]*101,100,
                                pair_order='problem-major',core_counts=[8,12]*101)
            metadata=json.loads((assets/'metadata.json').read_text())
            self.assertEqual([len(ids) for ids in metadata['planned_batch_tasks']],[100,100,1,1])
            for index, cores, socket in [(0,8,0),(1,12,1),(2,8,0),(3,12,1)]:
                plan=(assets/'plans'/f'batch_{index:06d}.sh').read_text()
                self.assertIn(f'MAX_CORES={cores}',plan)
                self.assertIn(f'EXCLUSIVE_CPU={socket}',plan)

    def test_solver_major_packs_interleaved_solvers_by_allocation(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = self.prepare(Path(folder), [(False, False)] * 202,
                                  100, pair_order='solver-major')
            metadata = json.loads((assets/'metadata.json').read_text())
            expected = [list(range(100)), list(range(100,200)), [200,201]]
            self.assertEqual(metadata['planned_batch_tasks'], expected)
            for index, ids in enumerate(expected):
                plan = (assets/'plans'/f'batch_{index:06d}.sh').read_text()
                task_ids = plan.split('TASK_IDS=(\n',1)[1].split(')',1)[0]
                self.assertEqual([int(value) for value in task_ids.split()],ids)

    def test_solver_major_keeps_different_requests_of_the_same_solver_separate(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = self.prepare(Path(folder), [(False,False)] * 8, 100,
                                  pair_order='solver-major',core_counts=[1,1,2,2,1,1,2,2])
            metadata = json.loads((assets/'metadata.json').read_text())
            self.assertEqual(metadata['planned_batch_tasks'], [[0,1,4,5],[2,3,6,7]])

    def test_partial_solver_major_batch_is_filled_by_the_next_compatible_solver(self):
        with tempfile.TemporaryDirectory() as folder:
            overrides=[{'solver_name':name} for name in ['A','A','A','B','B','B']]
            assets=self.prepare(Path(folder),[(False,False)]*6,4,pair_order='solver-major',overrides=overrides)
            metadata=json.loads((assets/'metadata.json').read_text())
            self.assertEqual(metadata['planned_batch_tasks'],[[0,1,2,3],[4,5]])

    def test_socket_and_thread_requests_still_require_separate_batches(self):
        for variant in ({'cpus':1},{'threads_per_core':2}):
            with self.subTest(variant=variant),tempfile.TemporaryDirectory() as folder:
                assets=self.prepare(Path(folder),[(False,False)]*4,100,pair_order='solver-major',
                                    overrides=[{},variant,{},variant])
                metadata=json.loads((assets/'metadata.json').read_text())
                self.assertEqual(metadata['planned_batch_tasks'],[[0,2],[1,3]])

    def test_solver_major_mixes_commands_and_limits_but_preserves_each_pair(self):
        for variant in ({'cpu_limit': 5}, {'wc_limit': 20}, {'mem_limit': '200MiB'},
                        {'command': './solver --strategy=other {{problem}}'}, {'omit_cpu_limit': 'true'}):
            with self.subTest(variant=variant), tempfile.TemporaryDirectory() as folder:
                overrides = [dict(solver_name='Same',**(variant if i%2 else {})) for i in range(6)]
                assets = self.prepare(Path(folder), [(False,False)] * 6, 2,
                                      pair_order='solver-major',overrides=overrides)
                metadata = json.loads((assets/'metadata.json').read_text())
                self.assertEqual(metadata['planned_batch_tasks'], [[0,1],[2,3],[4,5]])
                tasks=[json.loads(line) for line in (assets/'manifest.jsonl').read_text().splitlines()]
                for index, task in enumerate(tasks):
                    for field,value in variant.items():
                        expected = (value if index%2 else {'cpu_limit':3,'wc_limit':6,'mem_limit':'100MiB',
                                    'command':'./solver {{problem}}','omit_cpu_limit':'false'}[field])
                        if field=='mem_limit':
                            from slurmy import memory_bytes
                            expected=memory_bytes(expected)
                        elif field=='omit_cpu_limit': expected=expected=='true'
                        self.assertEqual(task[field],expected)
                # Commands and per-pair limits still live in separate scripts/configs.
                for task in tasks:
                    config=(assets/'calls'/str(task['task_id'])/'config.sh').read_text()
                    self.assertIn(f"CPU_LIMIT={task['cpu_limit']}\n",config)
                    self.assertIn(f"WC_LIMIT={task['wc_limit']}\n",config)

    def test_worker_keeps_solver_affinity_limit_with_socket_or_node_isolation(self):
        if not hasattr(os, 'sched_getaffinity') or not shutil.which('taskset'):
            self.skipTest('Linux affinity tools required')
        cpus=sorted(os.sched_getaffinity(0))[:4]
        if len(cpus)<4:self.skipTest('Four allowed CPUs required')
        for socket,node,allocated in [(False,False,1),(True,False,2),(False,True,4),(True,True,4)]:
            with self.subTest(socket=socket,node=node), tempfile.TemporaryDirectory() as folder:
                root=Path(folder);(root/'batches').mkdir();(root/'results').mkdir();(root/'csv.sh').touch()
                (root/'batches/batch_000000.sh').write_text(
                    f'TASK_IDS=(0 1)\nTASK_CORES=(1 1)\nTASK_CPUS=(1 1)\nTASK_THREADS_PER_CORE=(1 1)\n'
                    f'PARALLEL=1\nEXCLUSIVE_CPU={int(socket)}\nEXCLUSIVE_NODE={int(node)}\nRESERVED_CORES={allocated}\n')
                (root/'call.sh').write_text('python3 -c \'import os; print(len(os.sched_getaffinity(0)))\' >> "$JOB_DIR/affinity.txt"\n')
                bin_dir=root/'bin';bin_dir.mkdir()
                topology='\n'.join(f'{cpu},{index},{index//2}' for index,cpu in enumerate(cpus))
                (bin_dir/'lscpu').write_text("#!/bin/sh\ncat <<'EOF'\n"+topology+'\nEOF\n')
                (bin_dir/'lscpu').chmod(0o755)
                subprocess.run(['taskset','-c',','.join(map(str,cpus[:allocated])),'bash',
                    str(REPO/'implementation/templates/batch.sh')],check=True,capture_output=True,text=True,
                    env={**os.environ,'PATH':str(bin_dir)+':'+os.environ['PATH'],'SLURM_SUBMIT_DIR':str(root),'SLURM_ARRAY_TASK_ID':'0'})
                self.assertEqual((root/'affinity.txt').read_text().splitlines(),['1','1'])

    def test_remote_submission_preserves_sequential_plan_and_exclusive_allocation(self):
        cases=[(flags,(6,3,'100MiB'),'UNLIMITED',None) for flags in ([(False,False)]*2,[(True,False)]*2,[(False,True)]*2)]
        cases += [([(False,False)]*2,limits,maximum,None) for limits,maximum in [
            ((0,0,'0'),'UNLIMITED'), ((0,3,'100MiB'),'6:00:00'),
            ((0,0,'0'),'2-03:04:05'), ((6,0,'0'),'6:00:00')]]
        cases += [([(False,False)]*2,(6,3,'100MiB'),'UNLIMITED',overrides) for overrides in [
            [{'wc_limit':3,'cpu_limit':1},{'wc_limit':121,'cpu_limit':8,'mem_limit':'200MiB'}],
            [{'wc_limit':0,'mem_limit':'0'},{'wc_limit':121,'mem_limit':'200MiB'}]]]
        for flags,limits,maximum,overrides in cases:
            with self.subTest(flags=flags,limits=limits,maximum=maximum,overrides=overrides), tempfile.TemporaryDirectory() as folder:
                root=Path(folder)
                source=root/'workflow';source.mkdir()
                assets=self.prepare(source,flags,100,limits=limits,pair_order='solver-major',overrides=overrides)
                home=root/'home'
                job=home/'slurmy/jobs/test-job'
                shutil.copytree(assets,job)
                (job/'.inputs-ready').touch()
                # Capture scheduler arguments without allocating real cluster resources.
                (job/'submit_coordinator.py').write_text('''import json,sys
from pathlib import Path
if sys.argv[1]=='attempt':
 Path('submitted-options.json').write_text(json.dumps(sys.argv[5:]))
 print('ACCEPTED|12345|test capacity')
''')
                bin_dir=root/'bin';bin_dir.mkdir()
                (bin_dir/'scontrol').write_text('''#!/bin/bash
if [[ "$2" == config ]]; then
 echo 'SelectTypeParameters = CR_CORE_MEMORY'
 echo 'TaskPlugin = task/cgroup'
else
 echo 'OverSubscribe=NO MaxTime=MAXIMUM'
fi
''')
                control=bin_dir/'scontrol';control.write_text(control.read_text().replace('MAXIMUM',maximum))
                (bin_dir/'sinfo').write_text('#!/bin/bash\necho "2 8 2 64000"\n')
                for executable in bin_dir.iterdir():executable.chmod(0o755)
                subprocess.run(['bash',str(REPO/'implementation/templates/remote_submit.sh'),
                    'test-job','CPU-amd','32','test-job',base64.b64encode(b'Test job').decode(),'0','-','0','cpuopt'],
                    env={**os.environ,'HOME':str(home),'PATH':str(bin_dir)+':'+os.environ['PATH']},
                    check=True,capture_output=True,text=True)
                options=json.loads((job/'submitted-options.json').read_text())
                self.assertIn('--account=cpuopt',options)
                self.assertIn('SAVED_ACCOUNT=cpuopt',(job/'submission_context.sh').read_text())
                self.assertEqual('--exclusive' in options,any(node for _,node in flags))
                isolation='node' if any(node for _,node in flags) else 'socket' if any(socket for socket,_ in flags) else 'shared'
                self.assertIn(f'--comment=slurmy:v1:calls=2:batches=1:cores=2:isolation={isolation}:batch=0',options)
                self.assertEqual(any(o.startswith('--cores-per-socket=') for o in options),any(socket for socket,_ in flags))
                plan=job/'batches/batch_000000.sh'
                output=subprocess.check_output(['bash','-c','source "$1"; printf "%s %s %s" "${#TASK_IDS[@]}" "$PARALLEL" "$MEMORY_MIB"','test',str(plan)],text=True)
                from slurmy import memory_bytes
                pair_memory=[memory_bytes(item.get('mem_limit',limits[2])) for item in (overrides or [{},{}])]
                expected_memory=16000 if 0 in pair_memory else max(pair_memory)//2**20+128
                self.assertEqual(output,'2 1 '+str(expected_memory))
                pair_wall=[item.get('wc_limit',limits[0]) for item in (overrides or [{},{}])]
                expected_time=(60+sum(value+80+35 for value in pair_wall)+59)//60 if all(pair_wall) else 10080
                if maximum!='UNLIMITED':
                    from slurm_time_limits import seconds
                    expected_time=min(expected_time,seconds(maximum)//60)
                self.assertIn('--time='+str(expected_time),options)
                self.assertIn('--mem='+str(expected_memory)+'M',options)
                self.assertEqual(len(list((job/'batches').glob('batch_*.sh'))),1)


if __name__=='__main__':unittest.main()
