"""Timestamped download snapshots retain an exact host/job identity for deletion."""
import _bootstrap  # noqa: F401
import json
from pathlib import Path
import tempfile
import unittest
from download_records import record, snapshots


class DownloadRecordTests(unittest.TestCase):
    def test_multiple_modes_and_snapshots_match_only_the_same_host_and_job(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'job-results';root.mkdir()
            wanted=[]
            for name in ['Name_timestamp','verbose/Name_timestamp','starexec/Name_timestamp']:
                path=root/name;path.mkdir(parents=True);wanted.append(path)
                record(root,path,'datalab','job_123')
            other=root/'other';other.mkdir();record(root,other,'other-host','job_123')
            self.assertEqual(set(snapshots(root,'datalab','job_123')),set(wanted))
            for path in wanted:self.assertEqual(list(path.iterdir()),[])
            self.assertEqual(snapshots(root,'datalab','different-job'),[])

    def test_corrupt_records_and_outside_paths_or_symlinks_are_not_deleted(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)/'results';root.mkdir();registry=root/'.downloads';registry.mkdir()
            outside=Path(folder)/'keep';outside.mkdir()
            record(root,outside,'host','job')
            for name,value in [('invalid','bad json'),('outside',json.dumps(dict(host='host',job_id='job',destination=str(outside))))]:
                (registry/(name+'.json')).write_text(value)
            alias=root/'alias';alias.symlink_to(outside,target_is_directory=True)
            (registry/'link.json').write_text(json.dumps(dict(host='host',job_id='job',destination=str(alias))))
            self.assertEqual(snapshots(root,'host','job'),[])
            self.assertTrue(outside.is_dir())

    def test_older_compact_and_verbose_snapshots_use_existing_identity(self):
        import hashlib
        import os
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);compact=root/'old-name';compact.mkdir()
            identity=root/('.slurmy-export-identity-'+hashlib.sha256(os.fsencode(compact)).hexdigest()+'.json')
            identity.write_text(json.dumps(dict(host='host',job_id='job')))
            verbose=root/'verbose'/'old-name';verbose.mkdir(parents=True)
            (verbose/'sync-metadata.json').write_text(json.dumps(dict(host='host',slurmy_job_id='job')))
            self.assertEqual(set(snapshots(root,'host','job')),{compact,verbose})
            self.assertEqual(snapshots(root,'other','job'),[])

    def test_delete_cli_removes_registered_snapshots_only_after_remote_success(self):
        import contextlib
        import importlib.util
        import io
        import sys
        from types import SimpleNamespace
        from unittest.mock import patch
        spec=importlib.util.spec_from_file_location('download_cleanup_cli',Path(__file__).parents[1]/'slurmy-job.py')
        cli=importlib.util.module_from_spec(spec);spec.loader.exec_module(cli)
        with tempfile.TemporaryDirectory() as folder:
            snapshot=Path(folder)/'saved';snapshot.mkdir();(snapshot/'results').write_text('data')
            with patch.object(sys,'argv',['slurmy-job.py','delete','download-cleanup-fixture','--delete-local']), \
                 patch.object(cli,'snapshots',return_value=[snapshot]),patch('local_dispatch.stop'), \
                 patch('slurm_api.enabled',return_value=False),patch.object(cli.subprocess,'run',return_value=SimpleNamespace(returncode=0)), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(cli.main(),0)
            self.assertFalse(snapshot.exists())

    def test_deleted_snapshots_no_longer_offer_local_cleanup(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);path=root/'saved';path.mkdir();record(root,path,'host','job')
            path.rmdir();self.assertEqual(snapshots(root,'host','job'),[])
