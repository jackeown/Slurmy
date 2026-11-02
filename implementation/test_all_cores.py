"""All cores stays symbolic in definitions and resolves for the target partition."""
import csv
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from check_threads import dedicated_core_topology
from slurmy import read_pairs, SlurmyError
import test_batching


class AllCoresTests(unittest.TestCase):
    def test_resolves_socket_and_node_and_rejects_shared_cores(self):
        for socket, node, expected in [(True, False, 32), (True, True, 64), (False, True, None)]:
            with self.subTest(socket=socket,node=node), tempfile.TemporaryDirectory() as folder:
                root=Path(folder)
                test_batching.SequentialBatchTests().prepare(root,[(socket,node)]*2,100)
                path=root/'jobpairs.csv'
                with path.open() as stream:
                    reader=csv.DictReader(stream);columns=reader.fieldnames;rows=list(reader)
                for row in rows:row['cores']='all'
                with path.open('w') as stream:
                    writer=csv.DictWriter(stream,fieldnames=columns);writer.writeheader();writer.writerows(rows)
                with patch('check_threads.dedicated_core_topology',return_value=(2,32)) as topology:
                    if expected is None:
                        with self.assertRaisesRegex(SlurmyError,'requires dedicated'):read_pairs(path,'test-host','test-partition')
                        topology.assert_not_called()
                    else:
                        tasks=read_pairs(path,'test-host','test-partition')
                        self.assertEqual([t['cores'] for t in tasks],[expected]*2)
                        topology.assert_called_once_with('test-host','test-partition')
                        self.assertTrue(all(row['cores']=='all' for row in rows))

    def test_all_requires_complete_homogeneous_topology(self):
        for output, expected in [('2 32\n2 32\n',(2,32)),('2 32\n2 64\n',None),('',None),('0 32\n',None)]:
            with self.subTest(output=output), patch('check_threads.subprocess.run',return_value=subprocess.CompletedProcess([],0,output,'')):
                if expected:self.assertEqual(dedicated_core_topology('datalab','CPU-amd'),expected)
                else:
                    with self.assertRaises(ValueError):dedicated_core_topology('datalab','CPU-amd')
