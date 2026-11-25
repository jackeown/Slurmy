"""Prepare-only inspection reads the saved membership/order, with no SSH."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from slurmy_web import prepared_batches, workflows
from slurmy_web.app import create_app


class PreparedBatchTests(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name);self.workflow=self.root/'example';self.workflow.mkdir()
        (self.workflow/'Makefile').touch()
        assets=self.workflow/'submit.sh.files';assets.mkdir()
        self.metadata=assets/'metadata.json';self.manifest=assets/'manifest.jsonl'
        self.metadata.write_text(json.dumps(dict(task_count=4,batch_size=2,planned_batch_tasks=[[2,0],[3,1]])))
        self.manifest.write_text(''.join(json.dumps(dict(task_id=i,solver='A' if i%2 else 'B',problem=f'/Problems/P_{i}^1.p',command='./solver {{problem}}'))+'\n' for i in range(4)))
        (self.workflow/'submit.sh').touch()
        prepared_batches._read.cache_clear()
        with patch.object(workflows,'USER_RUNS',self.root),patch.object(workflows,'migrate_user_runs'):
            self.app=create_app()
        self.client=self.app.test_client()
        patcher=patch.object(workflows,'USER_RUNS',self.root);patcher.start();self.addCleanup(patcher.stop)

    def get(self,suffix=''):
        return self.client.get('/api/workflow/prepared-batches'+suffix,query_string={'directory':str(self.workflow),'per_page':1})

    def test_exact_prepared_order_pagination_and_download_without_submission(self):
        result=self.get();self.assertEqual(result.status_code,200,result.json)
        self.assertEqual(result.json['total'],2);self.assertEqual(result.json['batches'][0]['state'],'Prepared')
        pairs=self.get('/0').json
        self.assertEqual(pairs['tasks'][0]['id'],2);self.assertEqual(pairs['tasks'][0]['position'],1)
        response=self.client.get('/api/workflow/prepared-batches/0',query_string={'directory':str(self.workflow),'page':1,'per_page':1})
        self.assertEqual(response.json['tasks'][0]['id'],0)
        download=self.get('/download')
        rows=list(csv.DictReader(io.StringIO(download.text)))
        self.assertEqual([row['pair_id'] for row in rows],['2','0','3','1'])
        self.assertEqual(rows[0]['problem'],'/Problems/P_2^1.p')
        self.assertFalse((self.workflow/'submission.csv').exists())

    def test_invalidated_or_changed_preparation_never_appears_current(self):
        job,info=prepared_batches.snapshot(self.workflow);self.assertFalse(info['stale'])
        prepared=(self.workflow/'submit.sh').stat().st_mtime
        os.utime(self.workflow/'Makefile',(prepared+10,prepared+10))
        self.assertTrue(prepared_batches.snapshot(self.workflow)[1]['stale'])
        (self.workflow/'submit.sh').unlink()
        response=self.get();self.assertEqual(response.status_code,400)
        self.assertIn('Prepare only',response.json['error'])

    def test_regeneration_reloads_saved_plan_and_missing_tasks_fail_clearly(self):
        first,_=prepared_batches.snapshot(self.workflow)
        self.metadata.write_text(json.dumps(dict(task_count=4,planned_batch_tasks=[[3,2,1,0]])))
        second,_=prepared_batches.snapshot(self.workflow)
        self.assertIsNot(first,second)
        self.assertEqual(prepared_batches.pairs(second,0,0,4)['tasks'][0]['id'],3)
        self.metadata.write_text(json.dumps(dict(planned_batch_tasks=[[999]])))
        with self.assertRaisesRegex(ValueError,'incomplete'):prepared_batches.snapshot(self.workflow)
