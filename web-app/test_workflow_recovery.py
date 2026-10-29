"""Recovery is local, reversible, confined to the archive, and conflict safe."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from slurmy_web import workflows,recovery
from slurmy_web.app import create_app
from slurmy import read_builds


class WorkflowRecoveryTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        for name,value in [('USER_RUNS',self.root),('DELETED',self.root/'.deleted-workflows')]:
            patcher=patch.object(workflows,name,value);patcher.start();self.addCleanup(patcher.stop)
        self.source=self.root/'recover-me';self.source.mkdir()
        (self.source/'Makefile').write_text('all:\n\ttrue\n')
        (self.source/'source.c').write_text('saved source')
        with patch.object(workflows,'migrate_user_runs'):self.client=create_app().test_client()
        with self.client.session_transaction() as session:session['csrf']='test'
        self.headers={'X-CSRF-Token':'test'}

    def test_inspect_restore_and_conflict_preserve_files(self):
        archived=workflows.delete_workflow(self.source)
        listing=self.client.get('/api/workflows/recovery').json['entries']
        self.assertEqual(listing[0]['name'],'recover-me')
        url='/api/workflows/recovery/'+archived.name
        detail=self.client.get(url).json
        self.assertIn('source.c',[item['path'] for item in detail['files']])
        self.source.mkdir()
        self.assertEqual(self.client.post(url,json={'action':'restore'},headers=self.headers).status_code,400)
        self.assertTrue(archived.exists());self.source.rmdir()
        response=self.client.post(url,json={'action':'restore'},headers=self.headers)
        self.assertEqual(response.status_code,200)
        self.assertEqual((self.source/'source.c').read_text(),'saved source')
        self.assertFalse(archived.exists())
        self.assertEqual(self.client.get('/api/workflows/recovery').json['entries'],[])

    def test_remove_requires_confirmation_and_never_follows_links(self):
        outside=self.root/'outside';outside.mkdir();(outside/'keep').touch()
        (self.source/'resource').symlink_to(outside,target_is_directory=True)
        archived=workflows.delete_workflow(self.source)
        url='/api/workflows/recovery/'+archived.name
        detail=self.client.get(url).json
        self.assertEqual(next(item['kind'] for item in detail['files'] if item['path']=='resource'),'link')
        self.assertFalse(any('keep' in item['path'] for item in detail['files']))
        for data in ({'action':'remove'},{'action':'remove','name':'wrong'}):
            self.assertEqual(self.client.post(url,json=data,headers=self.headers).status_code,400)
        self.assertEqual(self.client.post(url,json={'action':'remove','name':'recover-me'},headers=self.headers).status_code,200)
        self.assertTrue((outside/'keep').exists());self.assertFalse(archived.exists())

    def test_symlink_archive_and_traversal_are_rejected(self):
        workflows.DELETED.mkdir()
        linked=workflows.DELETED/'linked-1793277447000000000'
        linked.symlink_to(self.source,target_is_directory=True)
        self.assertEqual(recovery.entries(),[])
        for value in (linked.name,'../recover-me','x/../recover-me','x\\y-1793277447000000000'):
            with self.assertRaises(ValueError):recovery.entry(value)
        self.assertEqual(self.client.post('/api/workflows/recovery/'+linked.name,json={'action':'restore'}).status_code,403)

    def test_duplicate_example_preserves_paths_and_all_build_settings(self):
        source=workflows.REPO/'workflows/example-workflows/example-shared-tptp'
        original=workflows.decisions(source,check_paths=False)
        destination=workflows.duplicate(source,'shared-tptp-copy')
        builds=read_builds(destination/'building.txt',check_paths=False)
        self.assertEqual(len(builds),len(original['resources']))
        for build,resource in zip(builds,original['resources']):
            self.assertEqual(str(build.root),resource['root'])
            self.assertEqual(build.artifact,resource['artifact'])
            self.assertEqual(str(build.source) if build.source else '',resource['source'])
            self.assertEqual(build.build_policy,resource['build_policy'])
        rows=workflows.imported_rows(destination/'configurations.csv',True,check_paths=False)
        self.assertEqual(rows[0]['solver_directory'],original['configurations'][0]['solver_directory'])
        self.assertEqual((destination/'problem-globs.txt').read_text().splitlines(),original['globs'])
        self.assertFalse((destination/'submit.sh').exists())
        with self.assertRaises(ValueError):workflows.duplicate(source,'shared-tptp-copy')

    def test_all_examples_can_be_duplicated_without_cluster_queries(self):
        examples=workflows.REPO/'workflows/example-workflows'
        with patch.object(workflows,'check_requested',side_effect=AssertionError('Duplication must remain local')):
            for source in sorted(examples.iterdir()):
                if not (source/'Makefile').is_file():continue
                with self.subTest(example=source.name):
                    destination=workflows.duplicate(source,source.name+'-copy')
                    self.assertTrue((destination/'Makefile').is_file())
                    self.assertTrue((destination/'.slurmy-workflow.json').is_file())
                    self.assertFalse((destination/'submit.sh.files').exists())

    def test_rename_updates_prepared_paths_bundles_and_history_without_ssh(self):
        import tarfile
        source=workflows.duplicate(workflows.REPO/'workflows/example-workflows/example-shared-tptp','original')
        assets=source/'submit.sh.files';(assets/'calls/0').mkdir(parents=True)
        original_path=str(source)
        contents=f'ROOT={original_path}/bin\nREL={original_path.lstrip("/")}/bin\nOTHER={original_path}-other/bin\n'
        (assets/'calls/0/config.sh').write_text(contents)
        (assets/'call-bundles').mkdir()
        with tarfile.open(assets/'call-bundles/batch_000000.tar.gz','w:gz') as archive:
            archive.add(assets/'calls/0/config.sh',arcname='0/config.sh')
        (assets/'archive-paths.txt').write_bytes((original_path+'/bin').encode()+b'\0')
        script=source/'submit.sh';script.write_text(contents);script.chmod(0o755)
        with patch.object(workflows,'check_requested',side_effect=AssertionError('Rename must remain local')):
            destination=workflows.rename_workflow(source,'original-new')
        expected=contents.replace(original_path+'/bin',str(destination)+'/bin').replace(original_path.lstrip('/')+'/bin',str(destination).lstrip('/')+'/bin')
        self.assertEqual((destination/'submit.sh').read_text(),expected)
        self.assertEqual((destination/'submit.sh').stat().st_mode & 0o777,0o755)
        with tarfile.open(destination/'submit.sh.files/call-bundles/batch_000000.tar.gz') as archive:
            self.assertEqual(archive.extractfile('0/config.sh').read().decode(),expected)
        data=json.loads((destination/'.slurmy-workflow.json').read_text())
        self.assertEqual(data['name'],'original-new')
        self.assertIn(original_path,data['_previous_directories'])
        self.assertFalse(source.exists())
        self.assertEqual((destination/'submit.sh.files/archive-paths.txt').read_bytes(),(str(destination)+'/bin').encode()+b'\0')
        self.source.mkdir(exist_ok=True)
        with self.assertRaises(ValueError):workflows.rename_workflow(destination,self.source.name)
        for name in ('../escape','bad name','A',''):
            with self.assertRaises(ValueError):workflows.rename_workflow(destination,name)
