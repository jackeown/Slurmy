"""Reservation propagation and shared-only workflow planning."""
import json
import csv
import io
import tarfile
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from slurmy_web import workflows
from slurmy import generate
from slurmy_archives import tptp_relative, browse_cluster_tptp


class ClusterSettingsTests(unittest.TestCase):
    def test_two_solvers_use_only_1429_selected_archive_problems(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            archive = root / 'SETProblems.tgz'
            with tarfile.open(archive, 'w:gz') as output:
                for index in range(1429):
                    body = b'fof(example,axiom,$true).\n'
                    info = tarfile.TarInfo(f'SET/SET{index:04d}+1.p')
                    info.size = len(body)
                    output.addfile(info, io.BytesIO(body))
            (root / 'unselected.p').touch()
            (root / 'limiter').touch()
            data = dict(name='email-regression', host='datalab', partition='CPU-amd',
                        mode='interactive', batch_size=500, pair_order='solver-major',
                        configurations=[dict(solver_name=name, solver_directory=str(root),
                            command='./solver {{problem}}', wc_limit=6, cpu_limit=3,
                            mem_limit='100MiB', cores=8) for name in ('VampireCASC', 'BuiltE')],
                        globs=[str(archive)], building_mode='create',
                        resources=[dict(root=str(root), mode='none', role='solver')],
                        limiter_mode='inline', limiter=f'{root}/limiter {{{{solver_command}}}}')
            with patch.object(workflows, 'check_requested'), patch('slurmy_archives.CACHE', root/'cache'):
                destination, files, count = workflows.specification(data, root/'workflow')
            self.assertEqual(count, 2858)
            rows = list(csv.DictReader(io.StringIO(files['jobpairs.csv'])))
            self.assertEqual([row['solver_name'] for row in rows], ['VampireCASC']*1429+['BuiltE']*1429)
            self.assertEqual(len({row['problem'] for row in rows}), 1429)
            self.assertTrue(all(Path(row['problem']).name.startswith('SET') for row in rows))
            destination.mkdir()
            for name, body in files.items():
                (destination/name).write_text(body)
            generate(destination/'jobpairs.csv', destination/'building.txt',
                     destination/'resource_limiter_template.txt', batch_size=500, pair_order='solver-major')
            metadata = json.loads((destination/'submit.sh.files/metadata.json').read_text())
            self.assertEqual(metadata['task_count'], 2858)
            self.assertEqual(metadata['batch_count'], 6)
            self.assertEqual([len(batch) for batch in metadata['planned_batch_tasks']], [500,500,429,500,500,429])
            for batch in metadata['planned_batch_tasks']:
                self.assertEqual(len({rows[index]['solver_name'] for index in batch}), 1)
            self.assertEqual(sorted(task for batch in metadata['planned_batch_tasks'] for task in batch), list(range(2858)))

    def test_thread_validation_checks_nodes_and_reports_errors(self):
        from slurmy_web.app import create_app
        with patch.object(workflows, 'migrate_user_runs'):
            app = create_app()
        client = app.test_client()
        with client.session_transaction() as session:
            session['csrf'] = 'test'
        def request(value):
            return client.post('/api/validate-threads', json=dict(host='datalab', partition='CPU-amd',
                               threads_per_core=value), headers={'X-CSRF-Token':'test'})
        with patch('check_threads.subprocess.run') as run:
            run.return_value.returncode = 0
            run.return_value.stdout = '2\n2\n'
            self.assertEqual(request(2).json, {'supported':2})
            self.assertEqual(request(3).status_code, 400)
            self.assertIn('supports 2', request(3).json['error'])
            run.return_value.stdout = '1\n2\n'
            self.assertIn('mixed', request(1).json['error'])
            run.return_value.returncode = 1
            run.return_value.stderr = 'Connection refused'
            self.assertIn('Connection refused', request(1).json['error'])
        for value in (0, -1, True, 1.5, ''):
            self.assertEqual(request(value).status_code, 400)

    def test_versioned_and_legacy_paths_and_traversal(self):
        self.assertEqual(tptp_relative('tptp://TPTP-v9.2.1/Problems/AGT/*.p'), 'TPTP-v9.2.1/Problems/AGT/*.p')
        self.assertEqual(tptp_relative('tptp://Problems/AGT/*.p'), 'TPTP-v9.2.1/Problems/AGT/*.p')
        self.assertEqual(tptp_relative('tptp://AGT/*.p'), 'TPTP-v9.2.1/Problems/AGT/*.p')
        for value in ('tptp://../secret', 'tptp://TPTP-v9.2.1/../secret', 'tptp://a\n'):
            with self.assertRaises(ValueError):
                tptp_relative(value)
        with self.assertRaises(ValueError):
            browse_cluster_tptp('tptp://TPTP-v9.2.1/Problems/*', 'test-host')

    def test_shared_only_workflow_reservation_builds_and_axiom_version(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / 'limiter').touch()
            recipe = root / 'recipe.sh'
            recipe.write_text('touch "$SLURMY_BUILD_OUTPUT/limiter"\n')
            problem = 'tptp:/share/TPTP/TPTP-v10/Problems/SET/example.p'
            data = dict(name='reservation-test', host='datalab', partition='CPU-amd',
                        reservation='E192-04', mode='interactive', batch_size=2, max_outstanding=32,
                        configurations=[dict(solver_name='Configured solver', solver_directory=str(root),
                                             command='./solver {{problem}}', wc_limit=6, cpu_limit=3,
                                             mem_limit='100MB', cores=1)],
                        globs=['tptp://TPTP-v10/Problems/SET/*.p'],
                        building_mode='create', resources=[dict(root=str(root), mode='script',
                            script=str(recipe), artifact='limiter', role='limiter')],
                        limiter_mode='inline', limiter=f'{root}/limiter {{{{solver_command}}}}')
            with patch.object(workflows, 'check_requested'), patch.object(workflows, 'problem_glob',
                    return_value=(data['globs'][0], [problem])):
                destination, files, count = workflows.specification(data, root / 'workflow')
            self.assertEqual(count, 1)
            self.assertIn('SLURMY_RESERVATION := E192-04', files['Makefile'])
            self.assertEqual(json.loads(files['.slurmy-workflow.json'])['reservation'], 'E192-04')
            destination.mkdir()
            for filename, body in files.items():
                (destination / filename).write_text(body)
            generate(destination/'jobpairs.csv', destination/'building.txt', destination/'resource_limiter_template.txt')
            assets = destination/'submit.sh.files'
            self.assertIn('TPTP_DEFAULT_ROOT=/share/TPTP/TPTP-v10', (assets/'calls/0/config.sh').read_text())
            # Run the generated build wrapper with a fake python; verify its actual argv.
            fakebin = root/'fakebin'
            fakebin.mkdir()
            (fakebin/'python').write_text('#!/bin/bash\nprintf "%s\\n" "$@"\n')
            (fakebin/'python').chmod(0o755)
            for reservation in ('', 'E192-04'):
                output = subprocess.check_output(['bash', str(assets/'builds/run.sh')], text=True,
                    env={**os.environ, 'PATH':str(fakebin)+':'+os.environ['PATH'],
                         'SLURMY_PARTITION':'CPU-amd', 'SLURMY_RESERVATION':reservation})
                self.assertEqual('--sbatch-option=--reservation=E192-04' in output, bool(reservation))
            data['reservation'] = 'bad;value'
            with self.assertRaises(ValueError):
                workflows.specification(data, root/'invalid')


if __name__ == '__main__':
    unittest.main()
