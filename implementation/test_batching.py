"""Sequential batching preserves exclusivity without splitting pairs."""
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
    def prepare(self, root, flags, size):
        problem = root/'example.p'
        problem.touch()
        (root/'limiter').touch()
        rows = []
        for i, (socket, node) in enumerate(flags):
            rows.append(dict(solver_name=f'Solver_{i%2}', command='./solver {{problem}}',
                solver_directory=str(root), problem=str(problem), wc_limit=6, cpu_limit=3,
                mem_limit='100MiB', cores=2 if i%2 else 1, threads_per_core=1,
                cpus='auto', exclusive_cpu=str(socket).lower(), exclusive_node=str(node).lower()))
        pairs=root/'jobpairs.csv'
        with pairs.open('w', newline='') as output:
            writer=csv.DictWriter(output, fieldnames=COLUMNS)
            writer.writeheader(); writer.writerows(rows)
        (root/'building.txt').write_text(json.dumps([{'root':str(root)}]))
        (root/'limiter.txt').write_text(f'{root}/limiter {{{{solver_command}}}}')
        generate(pairs,root/'building.txt',root/'limiter.txt',batch_size=size)
        return root/'submit.sh.files'

    def test_100_pairs_per_batch_even_with_alternating_socket_and_node_requests(self):
        with tempfile.TemporaryDirectory() as folder:
            assets=self.prepare(Path(folder),[(i%2==1,i%3==2) for i in range(201)],100)
            metadata=json.loads((assets/'metadata.json').read_text())
            self.assertEqual([len(ids) for ids in metadata['planned_batch_tasks']],[100,100,1])
            self.assertEqual(metadata['planned_batch_tasks'][0],list(range(100)))
            for i in (0,1):
                plan=(assets/'plans'/f'batch_{i:06d}.sh').read_text()
                self.assertIn('EXCLUSIVE_CPU=1',plan)
                self.assertIn('EXCLUSIVE_NODE=1',plan)

    def test_remote_submission_preserves_sequential_plan_and_exclusive_allocation(self):
        for flags in ([(False,False),(True,False)],[(False,False),(False,True)],[(False,False)]*2):
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as folder:
                root=Path(folder)
                source=root/'workflow';source.mkdir()
                assets=self.prepare(source,flags,100)
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
 echo 'OverSubscribe=NO'
fi
''')
                (bin_dir/'sinfo').write_text('#!/bin/bash\necho "2 8 2 64000"\n')
                for executable in bin_dir.iterdir():executable.chmod(0o755)
                subprocess.run(['bash',str(REPO/'implementation/templates/remote_submit.sh'),
                    'test-job','CPU-amd','32','test-job',base64.b64encode(b'Test job').decode()],
                    env={**os.environ,'HOME':str(home),'PATH':str(bin_dir)+':'+os.environ['PATH']},
                    check=True,capture_output=True,text=True)
                options=json.loads((job/'submitted-options.json').read_text())
                self.assertEqual('--exclusive' in options,any(node for _,node in flags))
                self.assertEqual(any(o.startswith('--cores-per-socket=') for o in options),any(socket for socket,_ in flags))
                plan=job/'batches/batch_000000.sh'
                output=subprocess.check_output(['bash','-c','source "$1"; printf "%s %s %s" "${#TASK_IDS[@]}" "$PARALLEL" "$MEMORY_MIB"','test',str(plan)],text=True)
                self.assertEqual(output,'2 1 228')
                self.assertEqual(len(list((job/'batches').glob('batch_*.sh'))),1)


if __name__=='__main__':unittest.main()
