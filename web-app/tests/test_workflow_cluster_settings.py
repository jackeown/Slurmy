"""Reservation propagation and shared-only workflow planning."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import json
import csv
import io
import tarfile
import os
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest
from unittest.mock import patch

from slurmy_web import workflows
from slurmy import generate
from slurmy_archives import tptp_relative, browse_cluster_tptp, tptp_collection_root, TPTP_DEFAULT_ROOT


class ClusterSettingsTests(unittest.TestCase):
    def test_batch_suggestion_uses_selected_global_host_without_workflow_host_field(self):
        from slurmy_web.app import create_app
        with patch.object(workflows,'migrate_user_runs'):
            client=create_app().test_client()
        with client.session_transaction() as session:session['csrf']='test'
        for query,payload,expected in [({'host':'datalabGeoff'},{},'datalabGeoff'),
                                       ({},{'host':'another-host'},'another-host'),
                                       ({'host':'datalabGeoff'},{'host':'old-host'},'datalabGeoff')]:
            with patch('slurmy_web.batch_suggestion.suggest',return_value={'batch_size':1}) as suggest:
                response=client.post('/api/suggest-batch-size',query_string=query,json=dict(partition='CPU-amd',**payload),headers={'X-CSRF-Token':'test'})
                self.assertEqual(response.status_code,200,response.json)
                self.assertEqual(suggest.call_args.args[0]['host'],expected)

    def test_new_dataset_root_and_computer_report_generation(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/'limiter').touch()
            problem = TPTP_DEFAULT_ROOT + '/Problems/SET/SET002+3.p'
            self.assertEqual(tptp_collection_root(problem), TPTP_DEFAULT_ROOT)
            self.assertIsNone(tptp_collection_root('/share/datasets/TPTP/TPTP-v10.0.0/../secret'))
            row = dict(solver_name='test', command='./solver {{problem}}', solver_directory=str(root),
                       problem='tptp:'+problem, wc_limit=3, cpu_limit=2, mem_limit='100MiB', cores=1,
                       threads_per_core=1, cpus='auto', exclusive_cpu='false', exclusive_node='false')
            (root/'solver-problem-pairs.csv').write_text(workflows.csv_text([row]))
            (root/'building.json').write_text(str(root)+'\n\n')
            for flag, enabled, expected in [('--timestamp', True, 1), ('', True, 0), ('--label=--timestamp', False, 0)]:
                (root/'resource-limiter.txt').write_text(f'{root}/limiter {flag} {{{{solver_command}}}}')
                generate(root/'solver-problem-pairs.csv', root/'building.json', root/'resource-limiter.txt', get_computer_info=enabled)
                assets = root/'submit.sh.files'
                config = (assets/'calls/0/config.sh').read_text()
                self.assertIn('TPTP_DEFAULT_ROOT='+TPTP_DEFAULT_ROOT, config)
                self.assertIn(f'GET_COMPUTER_INFO={int(enabled)}', config)
                self.assertIn(f'COMPUTER_INFO_TIMESTAMP={expected}', config)
                self.assertTrue((assets/'GetComputerInfo').is_file())
                shutil.rmtree(assets); (root/'submit.sh').unlink()

    def test_prebuilt_and_optional_limiter_build_settings_survive_workflow_generation(self):
        from slurmy import REPO
        limiter=REPO/'implementation/build/runsolver'
        self.assertTrue((limiter/'runsolver').is_file())
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);problem=root/'example.p';problem.write_text('cnf(a,axiom,p).')
            for mode,policy in [('none','changed'),('script','changed'),('script','always')]:
                data=dict(name='limiter-'+mode+'-'+policy,host='datalab',partition='CPU-amd',account='cpuopt',mode='interactive',
                    batch_size=1,configurations=[dict(solver_name='Test',solver_directory=str(root),command='./solver {{problem}}',
                        wc_limit=2,cpu_limit=1,mem_limit='100MiB',cores=1)],globs=[str(problem)],building_mode='create',
                    resources=[dict(role='solver',root=str(root),mode='none'),dict(role='limiter',root=str(limiter),mode=mode,
                        script=str(limiter/'build.sh'),source=str(limiter/'source'),artifact='runsolver',build_policy=policy)],
                    limiter_mode='inline',limiter=str(limiter/'runsolver')+' --cpu-limit {{cpu_limit}} {{solver_command}}')
                with patch.object(workflows,'check_requested'):
                    destination,files,count=workflows.specification(data,root/data['name'])
                self.assertIn('SLURMY_ACCOUNT := cpuopt',files['Makefile'])
                resource=json.loads(files['building.json'])[1]
                self.assertEqual(resource['build_policy'],policy)
                self.assertEqual(bool(resource.get('recipe')),mode=='script')
                if mode=='script':
                    self.assertEqual(resource['source'],str(limiter/'source'))
                    self.assertEqual(resource['artifact'],'runsolver')
                destination.mkdir()
                for name,body in files.items():(destination/name).write_text(body)
                generate(destination/'solver-problem-pairs.csv',destination/'building.json',destination/'resource-limiter.txt')
                settings=(destination/'submit.sh.files/submission_settings.sh').read_text()
                if mode=='script':
                    self.assertIn('--sbatch-option=--account=$SLURMY_ACCOUNT',(destination/'submit.sh.files/builds/run.sh').read_text())
                self.assertIn('BUILD_COUNT_DEFAULT='+('0' if mode=='none' else '1'),settings)

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
            rows = list(csv.DictReader(io.StringIO(files['solver-problem-pairs.csv'])))
            self.assertEqual([row['solver_name'] for row in rows], ['VampireCASC']*1429+['BuiltE']*1429)
            self.assertEqual(len({row['problem'] for row in rows}), 1429)
            self.assertTrue(all(Path(row['problem']).name.startswith('SET') for row in rows))
            destination.mkdir()
            for name, body in files.items():
                (destination/name).write_text(body)
            generate(destination/'solver-problem-pairs.csv', destination/'building.json',
                     destination/'resource-limiter.txt', batch_size=500, pair_order='solver-major')
            metadata = json.loads((destination/'submit.sh.files/metadata.json').read_text())
            self.assertEqual(metadata['task_count'], 2858)
            self.assertEqual(metadata['batch_count'], 6)
            self.assertEqual([len(batch) for batch in metadata['planned_batch_tasks']], [500,500,500,500,500,358])
            self.assertEqual([len({rows[index]['solver_name'] for index in batch})
                              for batch in metadata['planned_batch_tasks']],[1,1,2,1,1,1])
            self.assertEqual(metadata['planned_batch_tasks'][2],list(range(1000,1500)))
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
        self.assertEqual(tptp_relative('tptp://Problems/AGT/*.p'), 'TPTP-v10.0.0/Problems/AGT/*.p')
        self.assertEqual(tptp_relative('tptp://AGT/*.p'), 'TPTP-v10.0.0/Problems/AGT/*.p')
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
                            script=str(recipe), artifact='limiter', role='limiter', build_policy='changed')],
                        limiter_mode='inline', limiter=f'{root}/limiter {{{{solver_command}}}}')
            with patch.object(workflows, 'check_requested'), patch.object(workflows, 'problem_glob',
                    return_value=(data['globs'][0], [problem])):
                destination, files, count = workflows.specification(data, root / 'workflow')
            self.assertEqual(count, 1)
            self.assertIn('SLURMY_RESERVATION := E192-04', files['Makefile'])
            self.assertEqual(json.loads(files['workflow.json'])['reservation'], 'E192-04')
            destination.mkdir()
            for filename, body in files.items():
                (destination / filename).write_text(body)
            generate(destination/'solver-problem-pairs.csv', destination/'building.json', destination/'resource-limiter.txt')
            self.assertEqual(json.loads(files['building.json'])[0]['build_policy'], 'changed')
            assets = destination/'submit.sh.files'
            self.assertIn('--build-policy changed', (assets/'builds/run.sh').read_text())
            self.assertIn('TPTP_DEFAULT_ROOT=/share/TPTP/TPTP-v10', (assets/'calls/0/config.sh').read_text())
            # Run the generated build wrapper with a fake python; verify its actual argv.
            fakebin = root/'fakebin'
            fakebin.mkdir()
            (fakebin/'python').write_text('#!/bin/bash\nprintf "%s\\n" "$@"\n')
            (fakebin/'python').chmod(0o755)
            for reservation in ('', 'E192-04'):
                output = subprocess.check_output(['bash', str(assets/'builds/run.sh')], text=True,
                    env={**os.environ, 'SLURMY_PYTHON':str(fakebin/'python'), 'PATH':str(fakebin)+':'+os.environ['PATH'],
                         'SLURMY_PARTITION':'CPU-amd', 'SLURMY_RESERVATION':reservation})
                self.assertEqual('--sbatch-option=--reservation=E192-04' in output, bool(reservation))
            data['reservation'] = 'bad;value'
            with self.assertRaises(ValueError):
                workflows.specification(data, root/'invalid')


if __name__ == '__main__':
    unittest.main()
