"""Portable bundles survive relocation and reject unsafe archives."""
import csv
import io
import json
import os
from pathlib import Path
import shutil
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
        (self.workflow/'building.txt').write_text(json.dumps(resources))
        row=dict(solver_name='Solver + config',command="'"+str(self.runtime/'solver')+"' {{problem}}",solver_directory=str(self.runtime),
                 problem=str(self.problem),wc_limit=6,cpu_limit=3,mem_limit='100MiB',cores=1,cpus='auto',threads_per_core=1,
                 exclusive_cpu='false',exclusive_node='false')
        (self.workflow/'jobpairs.csv').write_text(workflows.csv_text([row]))
        (self.workflow/'resource_limiter_template.txt').write_text("'"+str(self.runtime/'limiter')+"' {{solver_command}}")
        (self.workflow/'Makefile').write_text('SLURMY_HOST := test-host\nSLURMY_PARTITION := CPU-amd\nBATCH_SIZE := 100\nSLURMY_RESERVATION := E192-04\n')

    def bundle(self):
        # This local workflow's implicit shared axioms are absent on this test host.
        def remote(*args,stdout,**kwargs):
            with tarfile.open(fileobj=stdout,mode='w'):pass
            return type('Result',(),dict(returncode=0))()
        with patch.object(portable.subprocess,'run',side_effect=remote):
            return portable.export_bundle(self.workflow,'test-host')

    def test_round_trip_survives_original_input_removal_and_can_prepare(self):
        bundle=self.bundle();self.addCleanup(bundle.close)
        shutil.rmtree(self.runtime);shutil.rmtree(self.source);self.problem.unlink();shutil.rmtree(self.workflow)
        imported,count=portable.import_bundle(bundle,'restored')
        self.assertEqual(count,1)
        pairs=read_pairs(imported/'jobpairs.csv');builds=read_builds(imported/'building.txt')
        self.assertEqual(pairs[0]['solver'],'Solver + config')
        self.assertEqual(Path(pairs[0]['problem']).name,'SET_002+3.p')
        self.assertEqual((builds[0].source/'source.c').read_text(),'int main() {}')
        self.assertFalse((builds[0].root/'solver-link').is_symlink())
        self.assertTrue((builds[0].root/'solver').stat().st_mode & 0o100)
        self.assertNotIn(str(self.runtime)+"'",pairs[0]['command'])
        self.assertIn('SLURMY_RESERVATION := E192-04',(imported/'Makefile').read_text())
        generate(imported/'jobpairs.csv',imported/'building.txt',imported/'resource_limiter_template.txt',batch_size=100)
        # Re-exporting an imported workflow must also work.
        with patch.object(portable.subprocess,'run') as run:
            run.side_effect=lambda *args,stdout,**kwargs:self.empty_remote(stdout)
            second=portable.export_bundle(imported,'test-host');second.close()

    def test_import_retains_problem_major_batching_policy(self):
        makefile = self.workflow/'Makefile'
        makefile.write_text(makefile.read_text()+'PAIR_ORDER := problem-major\n')
        with self.bundle() as bundle:
            imported, _ = portable.import_bundle(bundle, 'problem-major')
        self.assertIn('PAIR_ORDER := problem-major', (imported/'Makefile').read_text())
        self.assertEqual(json.loads((imported/'.slurmy-workflow.json').read_text())['pair_order'], 'problem-major')

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
            response=client.post('/api/workflows/import',data={'name':'uploaded','workflow':(io.BytesIO(content),'workflow.slurmy.tgz')},headers={'X-CSRF-Token':'test'})
            self.assertEqual(response.status_code,201,response.json)
            self.assertEqual(response.json['count'],1)
            self.assertTrue((Path(response.json['directory'])/'jobpairs.csv').is_file())

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
