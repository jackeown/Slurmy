"""Portable bundles survive relocation and reject unsafe archives."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from slurmy import COLUMNS, generate, read_pairs, read_builds
from slurmy_web import workflows, portable


class PortableWorkflowTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.users=self.root/'users';self.users.mkdir()
        patcher=patch.object(workflows,'USER_RUNS',self.users);patcher.start();self.addCleanup(patcher.stop)
        self.workflow=self.root/'original';self.workflow.mkdir()
        self.runtime=self.root/'solver runtime';self.runtime.mkdir()
        self.source=self.root/'solver source';self.source.mkdir()
        (self.source/'source.c').write_text('int main() {}')
        (self.runtime/'solver').write_text('#!/bin/sh\ncat "$1"\n');(self.runtime/'solver').chmod(0o755)
        (self.runtime/'limiter').write_text('#!/bin/sh\n"$@"\n');(self.runtime/'limiter').chmod(0o755)
        (self.runtime/'solver-link').symlink_to('solver')
        self.problem=self.root/'SET_002+3.p';self.problem.write_text('cnf(a,axiom,p).')
        recipe=self.workflow/'build.sh';recipe.write_text('cp solver "$SLURMY_BUILD_OUTPUT/solver"\n')
        resources=[dict(root=str(self.runtime),source=str(self.source),recipe=str(recipe),artifact='solver')]
        (self.workflow/'building.json').write_text(json.dumps(resources))
        row=dict(solver_name='Solver + config',command="'"+str(self.runtime/'solver')+"' {{problem}}",solver_directory=str(self.runtime),
                 problem=str(self.problem),wc_limit=6,cpu_limit=3,mem_limit='100MiB',cores=1,cpus='auto',threads_per_core=1,
                 exclusive_cpu='false',exclusive_node='false')
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text([row]))
        (self.workflow/'resource-limiter.txt').write_text("'"+str(self.runtime/'limiter')+"' {{solver_command}}")
        (self.workflow/'Makefile').write_text('SLURMY_HOST := test-host\nSLURMY_PARTITION := CPU-amd\nBATCH_SIZE := 100\nSLURMY_RESERVATION := E192-04\nSLURMY_ACCOUNT := cpuopt\n')

    def bundle(self):
        # This local workflow's implicit shared axioms are absent on this test host.
        def remote(*args,stdout,**kwargs):
            with tarfile.open(fileobj=stdout,mode='w'):pass
            return type('Result',(),dict(returncode=0))()
        with patch.object(portable.subprocess,'run',side_effect=remote):
            return portable.export_bundle(self.workflow,'test-host')

    def test_legacy_filenames_export_to_canonical_import(self):
        from slurmy_web.workflow_paths import ALIASES
        for current,old in ALIASES.items():
            path=self.workflow/current
            if path.exists():path.rename(self.workflow/old)
        bundle=self.bundle();self.addCleanup(bundle.close)
        imported,count=portable.import_bundle(bundle,'legacy-import')
        self.assertEqual(count,1)
        for name in ('solver-problem-pairs.csv','building.json','resource-limiter.txt','workflow.json'):
            self.assertTrue((imported/name).is_file(),name)
        self.assertIsInstance(json.loads((imported/'building.json').read_text()),list)
        self.assertEqual(workflows.decisions(imported)['jobpairs'],str(imported/'solver-problem-pairs.csv'))

    def test_custom_postprocessor_folder_and_external_script_survive_move_and_copy(self):
        import shlex
        post = self.root/'post folder';post.mkdir()
        script = self.root/'external parser.py';script.write_text('print("{}")')
        (post/'settings.json').write_text('{}')
        rows = workflows.imported_rows(self.workflow/'solver-problem-pairs.csv', check_paths=False)
        rows[0].update(postprocessor_mode='custom', postprocessor_directory=str(post),
                       postprocessor_command=shlex.join(['python3', str(script), '--settings', str(post/'settings.json')]),
                       postprocessor_timeout=47)
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text(rows))
        bundle=self.bundle();self.addCleanup(bundle.close)
        shutil.rmtree(post);script.unlink()
        imported,_=portable.import_bundle(bundle,'post-import')
        row=workflows.imported_rows(imported/'solver-problem-pairs.csv')[0]
        self.assertEqual(row['postprocessor_timeout'],47)
        self.assertTrue((Path(row['postprocessor_directory'])/'settings.json').is_file())
        words=shlex.split(row['postprocessor_command'])
        self.assertTrue(Path(words[1]).is_file())
        builds=read_builds(imported/'building.json')
        self.assertTrue(any(Path(words[1]).is_relative_to(build.root) for build in builds))
        data=workflows.decisions(imported)
        with patch.object(workflows,'check_requested'):
            workflows.save(data,imported)
        self.assertEqual(workflows.imported_rows(imported/'solver-problem-pairs.csv')[0]['postprocessor_command'],row['postprocessor_command'])
        self.assertTrue(Path(words[3]).is_file())
        copy=workflows.copyable_solver_configurations(imported)[0]
        self.assertEqual(copy['postprocessor_command'],row['postprocessor_command'])
        self.assertEqual(copy['postprocessor_timeout'],'47')
        duplicate=workflows.duplicate(imported,'post-copy')
        copied=workflows.imported_rows(duplicate/'solver-problem-pairs.csv')[0]
        self.assertTrue(Path(copied['postprocessor_directory']).is_relative_to(duplicate))
        self.assertNotIn(str(imported),copied['postprocessor_command'])
        self.assertTrue(Path(shlex.split(copied['postprocessor_command'])[1]).is_file())

    def test_workflow_postprocessor_and_inheritance_are_portable(self):
        post=self.root/'workflow parser';post.mkdir();(post/'parse.py').write_text('print("{}")')
        data=workflows.decisions(self.workflow)
        row=workflows.imported_rows(self.workflow/'solver-problem-pairs.csv')[0]
        row.pop('problem');row['postprocessor_override']=False
        data.update(mode='interactive',configurations=[row],globs=[str(self.problem)],
                    postprocessor=dict(mode='custom',directory=str(post),command='python3 parse.py',timeout=71))
        (self.workflow/'workflow.json').write_text(json.dumps(data))
        bundle=self.bundle();self.addCleanup(bundle.close)
        shutil.rmtree(post)
        imported,_=portable.import_bundle(bundle,'inherited-post')
        restored=workflows.decisions(imported)
        self.assertEqual(restored['postprocessor']['mode'],'custom')
        self.assertTrue(Path(restored['postprocessor']['directory']).is_relative_to(imported))
        self.assertFalse(restored['configurations'][0]['postprocessor_override'])
        self.assertEqual(restored['configurations'][0]['postprocessor_timeout'],71)
        self.assertTrue((Path(restored['postprocessor']['directory'])/'parse.py').is_file())

    def test_old_csv_without_postprocessor_settings_inherits_workflow_default(self):
        row=workflows.imported_rows(self.workflow/'solver-problem-pairs.csv')[0]
        self.assertFalse(row['postprocessor_override'])
        effective=workflows.effective_postprocessor(row,{'postprocessor':{'mode':'none'}},check_paths=False)
        self.assertEqual(effective['postprocessor_mode'],'none')

    def test_old_configuration_defaults_to_bundled_postprocessor(self):
        row=workflows.imported_rows(self.workflow/'solver-problem-pairs.csv')[0]
        self.assertEqual(row['postprocessor_mode'],'default')
        self.assertEqual(row['postprocessor_timeout'],30)
        card=workflows.copyable_solver_configurations(self.workflow)[0]
        self.assertEqual(card['postprocessor_mode'],'default')
        self.assertEqual(card['postprocessor_timeout'],'30')

    def test_environment_survives_portable_export_and_configuration_copy(self):
        rows=workflows.imported_rows(self.workflow/'solver-problem-pairs.csv',check_paths=False)
        rows[0]['environment']='X=Y\nHello = "World with spaces"'
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text(rows))
        bundle=self.bundle();self.addCleanup(bundle.close)
        imported,_=portable.import_bundle(bundle,'environment-import')
        self.assertEqual(workflows.imported_rows(imported/'solver-problem-pairs.csv',check_paths=False)[0]['environment'],rows[0]['environment'])
        self.assertEqual(workflows.copyable_solver_configurations(imported)[0]['environment'],rows[0]['environment'])

    def test_explicit_problem_selection_order_survives_portable_workflow(self):
        second=self.root/'AAA.p';second.write_text('cnf(b,axiom,q).')
        data=workflows.decisions(self.workflow)
        configuration=workflows.imported_rows(self.workflow/'solver-problem-pairs.csv',check_paths=False)[0]
        configuration.pop('problem',None)
        data.update(mode='interactive',configurations=[configuration],globs=[str(self.root/'*.p')],problem_selections=[self.problem.name+'\n'+second.name])
        with patch.object(workflows,'check_requested'):
            workflows.save(data,self.workflow)
        bundle=self.bundle();self.addCleanup(bundle.close)
        imported,_=portable.import_bundle(bundle,'ordered')
        restored=workflows.decisions(imported)
        self.assertEqual([Path(path).name for path in restored['_problem_order']],[self.problem.name,second.name])
        with patch.object(workflows,'check_requested'):
            workflows.save(restored,imported)
        with (imported/'solver-problem-pairs.csv').open() as stream:rows=list(csv.DictReader(stream))
        self.assertEqual([Path(row['problem']).name for row in rows],[self.problem.name,second.name])

    def test_import_resolves_account_for_current_cluster_user(self):
        from slurmy_web import accounts
        for available,expected in [(['default'],'default'),(['other','default'],'other'),(['cpuopt','default'],'cpuopt'),(['research'],'research'),(['alpha','beta','default'],'default')]:
            with self.subTest(accounts=available):
                bundle=self.bundle();self.addCleanup(bundle.close)
                with patch.object(accounts,'memberships',return_value={'accounts':available}) as lookup:
                    imported,_=portable.import_bundle(bundle,'account-'+'-'.join(available),selected_host='current-host')
                lookup.assert_called_once_with('current-host')
                self.assertEqual(workflows.decisions(imported)['account'],expected)
                self.assertIn('SLURMY_ACCOUNT := '+expected,(imported/'Makefile').read_text())
                self.assertNotIn('host',json.loads((imported/'workflow.json').read_text()))

    def test_zero_limit_omission_choices_survive_portable_round_trip_and_copy(self):
        from slurmy import generate
        with (self.workflow/'solver-problem-pairs.csv').open() as stream:row=next(csv.DictReader(stream))
        row.update(cpu_limit='0',wc_limit='0',mem_limit='0',omit_cpu_limit='true',omit_wc_limit='true',omit_mem_limit='true',
                   command='./solver --cpu-limit {{cpu_limit}} --wall={{wc_limit}} --mem={{mem_limit_mib}} {{problem}}')
        (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text([row]))
        bundle=self.bundle();self.addCleanup(bundle.close)
        imported,_=portable.import_bundle(bundle,'zero-omissions')
        self.assertEqual(workflows.decisions(imported)['account'],'cpuopt')
        self.assertIn('SLURMY_ACCOUNT := cpuopt',(imported/'Makefile').read_text())
        with (imported/'solver-problem-pairs.csv').open() as stream:restored=next(csv.DictReader(stream))
        for field in ('omit_cpu_limit','omit_wc_limit','omit_mem_limit'):self.assertEqual(restored[field],'true')
        card=workflows.copyable_solver_configurations(imported)[0]
        self.assertEqual(card['cpu_limit'],'0');self.assertEqual(card['wc_limit'],'0');self.assertEqual(card['mem_limit'],'0')
        self.assertEqual(card['omit_cpu_limit'],'true')
        generate(imported/'solver-problem-pairs.csv',imported/'building.json',imported/'resource-limiter.txt')
        script=(imported/'submit.sh.files/calls/0/solver.sh').read_text()
        self.assertNotIn('--cpu-limit',script);self.assertNotIn('--wall=',script);self.assertNotIn('--mem=',script)

    def test_shared_custom_axioms_are_materialized_in_portable_bundle(self):
        remote=Path('tptp:/share/datasets/TPTP/TPTP-v9.2.1/Axioms/SYN001+0.ax')
        (self.workflow/'axiom-globs.txt').write_text('tptp://TPTP-v9.2.1/Axioms/SYN001*.ax\n')
        def fetch(*args,stdout,**kwargs):
            with tarfile.open(fileobj=stdout,mode='w') as archive:
                body=b'cnf(shared,axiom,p).';member=tarfile.TarInfo('assets'+str(remote)[5:]);member.size=len(body)
                archive.addfile(member,io.BytesIO(body))
            return type('Result',(),dict(returncode=0))()
        with patch.object(portable,'read_axioms',return_value=([remote],{remote:Path('SYN001+0.ax')})),patch.object(portable.subprocess,'run',side_effect=fetch):
            bundle=portable.export_bundle(self.workflow,'test-host')
        self.addCleanup(bundle.close)
        imported,_=portable.import_bundle(bundle,'custom-shared-axioms')
        axioms=(imported/'axiom-globs.txt').read_text()
        self.assertNotIn('tptp:',axioms)
        self.assertTrue(Path(axioms.strip()).is_file())
        self.assertEqual(Path(axioms.strip()).read_text(),'cnf(shared,axiom,p).')

    def test_computer_information_setting_survives_portable_round_trip(self):
        with (self.workflow/'Makefile').open('a') as output:
            output.write('GET_COMPUTER_INFO := false\n')
        bundle = self.bundle(); self.addCleanup(bundle.close)
        imported, _ = portable.import_bundle(bundle, 'without-computer-report')
        self.assertIs(workflows.decisions(imported, check_paths=False)['get_computer_info'], False)
        self.assertIn('GET_COMPUTER_INFO := false', (imported/'Makefile').read_text())

    def test_new_and_legacy_shared_collections_remain_portable(self):
        for index, root in enumerate(('/share/datasets/TPTP/TPTP-v10.0.0', '/share/TPTP/TPTP-v9.2.1')):
            with self.subTest(root=root):
                with (self.workflow/'solver-problem-pairs.csv').open() as source:
                    row = next(csv.DictReader(source))
                row['problem'] = 'tptp:' + root + '/Problems/SET/SET002+3.p'
                (self.workflow/'solver-problem-pairs.csv').write_text(workflows.csv_text([row]))
                def remote(*args, stdout, **kwargs):
                    with tarfile.open(fileobj=stdout, mode='w') as archive:
                        directory = tarfile.TarInfo('assets'+root+'/Axioms')
                        directory.type = tarfile.DIRTYPE; archive.addfile(directory)
                        for name, body in [('Problems/SET/SET002+3.p', b"include('Axioms/SET001+0.ax').\n"),
                                           ('Axioms/SET001+0.ax', b'fof(a,axiom,p).\n')]:
                            member = tarfile.TarInfo('assets'+root+'/'+name); member.size = len(body)
                            archive.addfile(member, io.BytesIO(body))
                    return type('Result', (), dict(returncode=0))()
                with patch.object(portable.subprocess, 'run', side_effect=remote):
                    bundle = portable.export_bundle(self.workflow, 'test-host')
                self.addCleanup(bundle.close)
                if index:
                    # Older exporters had neither the default-root nor computer-info fields.
                    legacy = io.BytesIO()
                    with tarfile.open(fileobj=bundle, mode='r:gz') as source, tarfile.open(fileobj=legacy, mode='w:gz') as target:
                        for member in source:
                            content = source.extractfile(member) if member.isfile() else None
                            if member.name == 'workflow.json':
                                payload = json.load(content)
                                payload.pop('default_tptp_root', None)
                                payload['settings'].pop('get_computer_info', None)
                                body = json.dumps(payload).encode(); member.size = len(body); content = io.BytesIO(body)
                            target.addfile(member, content)
                    legacy.seek(0); bundle = legacy
                imported, count = portable.import_bundle(bundle, 'shared-'+str(index))
                self.assertEqual(count, 1)
                pair = read_pairs(imported/'solver-problem-pairs.csv')[0]
                self.assertFalse(pair['remote_problem'])
                library = imported/'assets'/root.lstrip('/')
                self.assertEqual(Path(pair['problem']), library/'Problems/SET/SET002+3.p')
                self.assertIn(str(library), pair['command'])
                self.assertTrue((library/'Axioms/SET001+0.ax').is_file())
                self.assertTrue(workflows.decisions(imported, check_paths=False)['get_computer_info'])

    def test_round_trip_survives_original_input_removal_and_can_prepare(self):
        bundle=self.bundle();self.addCleanup(bundle.close)
        shutil.rmtree(self.runtime);shutil.rmtree(self.source);self.problem.unlink();shutil.rmtree(self.workflow)
        imported,count=portable.import_bundle(bundle,'restored')
        self.assertEqual(count,1)
        pairs=read_pairs(imported/'solver-problem-pairs.csv');builds=read_builds(imported/'building.json')
        self.assertEqual(pairs[0]['solver'],'Solver + config')
        self.assertEqual(Path(pairs[0]['problem']).name,'SET_002+3.p')
        self.assertEqual((builds[0].source/'source.c').read_text(),'int main() {}')
        self.assertFalse((builds[0].root/'solver-link').is_symlink())
        self.assertTrue((builds[0].root/'solver').stat().st_mode & 0o100)
        self.assertNotIn(str(self.runtime)+"'",pairs[0]['command'])
        self.assertIn('SLURMY_RESERVATION := E192-04',(imported/'Makefile').read_text())
        generate(imported/'solver-problem-pairs.csv',imported/'building.json',imported/'resource-limiter.txt',batch_size=100)
        # Re-exporting an imported workflow must also work.
        with patch.object(portable.subprocess,'run') as run:
            run.side_effect=lambda *args,stdout,**kwargs:self.empty_remote(stdout)
            second=portable.export_bundle(imported,'test-host');second.close()

    def test_solver_limiter_legacy_in_place_and_prebuilt_resources_survive_relocation(self):
        # Exercise every BuildSpec shape together, with a separately built limiter.
        (self.workflow/'build.sh').write_text('install -m 755 solver "$SLURMY_BUILD_OUTPUT/solver"\n')
        (self.source/'solver').write_text('#!/bin/sh\nprintf "%% SZS status Theorem\\n"\n')
        limiter_root = self.root/'limiter runtime'; limiter_root.mkdir()
        limiter_source = self.root/'limiter source'; limiter_source.mkdir()
        (limiter_source/'limiter').write_text('#!/bin/sh\n"$@"\n')
        limiter_recipe = self.workflow/'build-limiter.sh'
        limiter_recipe.write_text('install -m 755 limiter "$SLURMY_BUILD_OUTPUT/limiter"\n')
        resources = json.loads((self.workflow/'building.json').read_text())
        resources.append(dict(root=str(limiter_root),source=str(limiter_source),recipe=str(limiter_recipe),artifact='limiter'))
        for name, artifact in [('legacy resource', ''), ('in-place resource', 'built')]:
            root = self.root/name;root.mkdir()
            (root/'input').write_text(name)
            recipe = self.workflow/(name.replace(' ','-')+'.sh')
            recipe.write_text('cp input built\n')
            resources.append(dict(root=str(root), source=str(root), recipe=str(recipe), artifact=artifact, legacy=not artifact))
        prebuilt = self.root/'prebuilt';prebuilt.mkdir();(prebuilt/'data').write_text('prebuilt data')
        resources.append(dict(root=str(prebuilt), source='', recipe='', artifact=''))
        (self.workflow/'building.json').write_text(json.dumps(resources))
        (self.workflow/'resource-limiter.txt').write_text("'"+str(limiter_root/'limiter')+"' {{solver_command}}")
        axiom = self.root/'SET_001+0.ax';axiom.write_text('fof(ax,axiom,p).')
        (self.workflow/'axiom-globs.txt').write_text(str(axiom)+'\n')
        with self.bundle() as bundle:
            for path in {self.runtime,self.source,limiter_root,limiter_source,prebuilt,
                         self.root/'legacy resource',self.root/'in-place resource',self.workflow}:
                shutil.rmtree(path)
            self.problem.unlink();axiom.unlink()
            imported, count = portable.import_bundle(bundle, 'all-resource-modes')
        self.assertEqual(count,1)
        builds = read_builds(imported/'building.json')
        self.assertEqual(len(builds),5)
        generate(imported/'solver-problem-pairs.csv',imported/'building.json',imported/'resource-limiter.txt',
                 imported/'axiom-globs.txt')
        # Run the archived recipes in fresh build contexts just like the driver;
        # live verification also exercises the cluster driver itself.
        for index, build in enumerate(builds):
            if not build.recipe:
                self.assertEqual((build.root/'data').read_text(),'prebuilt data')
                continue
            work = self.root/f'work-{index}';output=self.root/f'output-{index}'
            shutil.copytree(build.source,work);output.mkdir()
            recipe = build.recipe if build.artifact else imported/f'submit.sh.files/builds/recipe_{index}.sh'
            subprocess.run(['bash',str(recipe)],cwd=work,check=True,
                           env={**os.environ,'SLURMY_BUILD_WORK':str(work),'SLURMY_BUILD_OUTPUT':str(output)})
            artifact_dir = work if build.source==build.root and build.artifact else output
            self.assertTrue((artifact_dir/(build.artifact or 'built')).is_file())
        self.assertIn(str(builds[1].root/'limiter'),(imported/'resource-limiter.txt').read_text())
        sources, _ = portable.read_axioms(imported/'axiom-globs.txt')
        self.assertEqual([path.name for path in sources],['SET_001+0.ax'])
        self.assertEqual(sources[0].read_text(),'fof(ax,axiom,p).')
        with portable.export_bundle(imported,'test-host') as second:
            self.assertGreater(len(second.read()),0)

    def test_import_retains_problem_major_batching_policy(self):
        makefile = self.workflow/'Makefile'
        makefile.write_text(makefile.read_text()+'PAIR_ORDER := problem-major\n')
        with self.bundle() as bundle:
            imported, _ = portable.import_bundle(bundle, 'problem-major')
        self.assertIn('PAIR_ORDER := problem-major', (imported/'Makefile').read_text())
        self.assertEqual(json.loads((imported/'workflow.json').read_text())['pair_order'], 'problem-major')

    def empty_remote(self,output):
        with tarfile.open(fileobj=output,mode='w'):pass
        return type('Result',(),dict(returncode=0))()

    def test_http_download_and_upload_accept_large_bundles(self):
        from slurmy_web.app import create_app
        (self.runtime/'payload.bin').write_bytes(os.urandom(3*1024**2))
        with patch.object(workflows, 'USER_RUNS', self.root), patch.object(workflows, 'migrate_user_runs'):
            app=create_app()
            client=app.test_client()
            with client.session_transaction() as session:session['csrf']='test'
            exported=client.post('/api/workflows/export',query_string={'directory':str(self.workflow),'host':'test-host'},headers={'X-CSRF-Token':'test'})
            self.assertEqual(exported.status_code,200)
            self.assertIn('original.slurmy.tgz',exported.headers['Content-Disposition'])
            content=exported.data;exported.close()
            self.assertGreater(len(content),2*1024**2)
            with patch('slurmy_web.accounts.memberships',return_value={'accounts':['default']}):
                response=client.post('/api/workflows/import',data={'name':'uploaded','workflow':(io.BytesIO(content),'workflow.slurmy.tgz')},headers={'X-CSRF-Token':'test'})
            self.assertEqual(response.status_code,201,response.json)
            self.assertEqual(response.json['count'],1)
            self.assertTrue((Path(response.json['directory'])/'solver-problem-pairs.csv').is_file())

    def test_large_upload_limit_does_not_change_other_requests(self):
        from flask import request
        from slurmy_web.app import create_app
        with patch.object(workflows, 'migrate_user_runs'):
            app = create_app()
        for path, limit in [('/api/workflows/import', 10 * 1024**3),
                            ('/api/workflows/files/replace', 64 * 1024**2),
                            ('/api/workflows/import-other', 2 * 1024**2),
                            ('/api/workflows/export', 2 * 1024**2)]:
            with app.test_request_context(path, method='POST'):
                self.assertEqual(request.max_content_length, limit)
        self.assertEqual(app.config['MAX_CONTENT_LENGTH'], 2 * 1024**2)

    def test_duplicate_name_and_unsafe_archive_do_not_overwrite(self):
        existing=self.users/'existing';existing.mkdir();(existing/'keep').write_text('keep')
        with self.assertRaises(ValueError):portable.import_bundle(io.BytesIO(), 'existing')
        self.assertEqual((existing/'keep').read_text(),'keep')
        for name,kind in [('../escape',tarfile.REGTYPE),('/absolute',tarfile.REGTYPE),('assets/link',tarfile.SYMTYPE),('assets/pipe',tarfile.FIFOTYPE)]:
            with self.subTest(name=name):
                stream=io.BytesIO()
                with tarfile.open(fileobj=stream,mode='w:gz') as archive:
                    member=tarfile.TarInfo(name);member.type=kind;member.linkname='/tmp';archive.addfile(member)
                stream.seek(0)
                with self.assertRaises(ValueError):portable.import_bundle(stream,'unsafe')
                self.assertFalse((self.users/'unsafe').exists())

    def test_command_relocation_quotes_paths_with_spaces(self):
        actual=portable.command_paths('/old/bin/solver --input /old/problem.p',{'/old/bin':'/new location/bin','/old/problem.p':'/new location/problem.p'})
        self.assertEqual(actual,"'/new location/bin/solver' --input '/new location/problem.p'")


if __name__=='__main__':unittest.main()
