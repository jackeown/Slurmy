"""Workflow deletion offers recovery or permanent removal, confined to local files."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from slurmy_web import workflows
from slurmy_web.app import create_app


class WorkflowDeletionTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        self.source=self.root/'test-workflow';self.source.mkdir()
        (self.source/'Makefile').write_text('all:\n\ttrue\n')
        (self.source/'source.c').write_text('source')
        self.archive=self.root/'.deleted-workflows'
        for name,value in [('USER_RUNS',self.root),('DELETED',self.archive)]:
            patcher=patch.object(workflows,name,value);patcher.start();self.addCleanup(patcher.stop)

    def test_archive_preserves_files(self):
        archived=workflows.delete_workflow(self.source)
        self.assertFalse(self.source.exists())
        self.assertEqual((archived/'source.c').read_text(),'source')

    def test_permanent_removal_does_not_follow_resource_symlinks(self):
        external=self.root/'outside';external.mkdir();(external/'keep').touch()
        (self.source/'linked-runtime').symlink_to(external,target_is_directory=True)
        self.assertIsNone(workflows.delete_workflow(self.source,permanent=True))
        self.assertFalse(self.source.exists());self.assertFalse(self.archive.exists())
        self.assertTrue((external/'keep').exists())

    def test_api_permanent_flag_and_confirmation(self):
        with patch.object(workflows,'migrate_user_runs'):
            client=create_app().test_client()
        with client.session_transaction() as session:session['csrf']='test'
        options=dict(query_string={'directory':str(self.source)},headers={'X-CSRF-Token':'test'})
        for data in ({'name':'wrong','permanent':True},{'name':'test-workflow','permanent':'true'}):
            response=client.post('/api/workflows/delete',json=data,**options)
            self.assertEqual(response.status_code,400)
            self.assertTrue(self.source.exists())
        response=client.post('/api/workflows/delete',json={'name':'test-workflow','permanent':True},**options)
        self.assertEqual(response.status_code,200)
        self.assertTrue(response.json['permanent']);self.assertIsNone(response.json['archived'])
        self.assertFalse(self.source.exists())
