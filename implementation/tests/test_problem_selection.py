"""Filename selections preserve source order through CLI preparation and batching."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import problem_selection
from slurmy import COLUMNS, order_jobpairs

spec=importlib.util.spec_from_file_location('selection_pairs',Path(__file__).parent.parent/'slurmy-pairs.py')
pairs=importlib.util.module_from_spec(spec);spec.loader.exec_module(pairs)


class ProblemSelectionTests(unittest.TestCase):
    def test_explicit_filename_order_missing_and_ambiguous_names(self):
        files=['/a/SET002+3.p','/a/SET002^3.p','/a/OTHER.p']
        self.assertEqual(problem_selection.merge([files],['SET002^3.p\nSET002+3.p']),[files[1],files[0]])
        self.assertEqual(problem_selection.merge([files],[]),sorted(files))
        with self.assertRaisesRegex(ValueError,'not found'):problem_selection.select(files,'missing.p')
        with self.assertRaisesRegex(ValueError,'ambiguous'):problem_selection.select(['/a/p.p','/b/p.p'],'p.p')

    def test_absolute_paths_match_exactly_and_relative_paths_match_basename(self):
        files=['/a/P.p','/b/P.p','/a/Q.p']
        self.assertEqual(problem_selection.select(files,'/b/P.p\nrelative/Q.p'),['/b/P.p','/a/Q.p'])
        with self.assertRaisesRegex(ValueError,'not found'):problem_selection.select(files,'/missing/Q.p')
        with self.assertRaisesRegex(ValueError,'ambiguous'):problem_selection.select(files,'relative/P.p')
        remote=['tptp:/share/datasets/TPTP/TPTP-v10.0.0/Problems/SET/SET001.p']
        self.assertEqual(problem_selection.select(remote,remote[0][5:]),remote)
        self.assertEqual(problem_selection.select(remote,'SET/SET001.p'),remote)

    def test_cli_and_problem_major_order_keep_selected_order(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name in ['A.p','B.p','C.p']:(root/name).touch()
            runtime=root/'runtime';runtime.mkdir()
            configuration=root/'configurations.csv'
            columns=[column for column in COLUMNS if column!='problem']
            with configuration.open('w',newline='') as stream:
                writer=csv.DictWriter(stream,fieldnames=columns);writer.writeheader()
                for solver in ['First','Second']:
                    writer.writerow(dict(solver_name=solver,solver_directory=str(runtime),command='./solver {{problem}}',wc_limit=1,cpu_limit=1,mem_limit='10MiB',cores=1,threads_per_core=1,cpus='auto',exclusive_cpu='false',exclusive_node='false'))
            output=root/'jobpairs.csv'
            pairs.expand([configuration],[str(root/'*.p')],output,'problem-major',['C.p\nA.p'])
            with output.open() as stream:rows=list(csv.DictReader(stream))
            self.assertEqual([Path(row['problem']).name for row in rows],['C.p','C.p','A.p','A.p'])
            self.assertEqual([row['solver_name'] for row in rows],['First','Second','First','Second'])
            self.assertEqual([row['problem'] for row in order_jobpairs(rows,'problem-major')],[row['problem'] for row in rows])
