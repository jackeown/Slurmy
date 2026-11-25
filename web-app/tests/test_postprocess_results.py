"""Postprocessor metadata stays independent of solver execution and SZS aliases."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import io
import json
import re
import subprocess
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from slurmy_web.app import call_outcome, history_outcomes, outcome_breakdown, task_page, create_app
from slurmy_web.monitor import (JobSnapshot, parse_csv_results, parse_jsonl_results,
                               summarize_results, parse_summary, REMOTE_SNAPSHOT_SCRIPT)


def row(status='sat', data=None):
    fields=['']*21
    fields[:5]=['0','true','ok','0','1.5']
    fields[17]=json.dumps(data if data is not None else {'status':status,'proof size':12,'nested':{'safe':'<script>'}})
    fields[18:]=['124','Postprocessor timed out',status]
    return fields


def csv_text(fields):
    stream=io.StringIO();csv.writer(stream,quoting=csv.QUOTE_ALL).writerow(fields);return stream.getvalue()


class PostprocessResultsTests(unittest.TestCase):
    def test_running_postprocessor_is_distinguished_from_solver_startup(self):
        from types import SimpleNamespace
        from slurmy_web.app import unfinished_call
        job=JobSnapshot('job','/jobs/job',0,metadata={'task_count':1})
        job.task_batches={0:0}
        record=SimpleNamespace(source='queue',state='RUNNING',array_job_id='123',array_task_id='0')
        state,_,reason,_=unfinished_call(job,0,SimpleNamespace(state='postprocessing'),{0:record})
        self.assertEqual(state,'postprocessing');self.assertIn('solver finished',reason)

    def test_custom_smt_status_survives_table_chart_cube_and_error(self):
        for status in ['sat','unsat','SAT','custom <result>, "quoted"','line\nbreak\tstatus']:
            result=parse_csv_results(csv_text(row(status)))[0]
            job=JobSnapshot('job','/jobs/job',0,metadata={'task_count':1})
            job.results={0:result};summarize_results(job,job.results)
            self.assertEqual(call_outcome(result),(status,'solver'))
            self.assertEqual(result.status,'ok')
            self.assertEqual(result.szs_status,'')
            rows,count=task_page(job,0,100,'state','asc',outcomes={(status,'solver')})
            self.assertEqual(count,1);self.assertEqual(rows[0]['state'],status)
            self.assertEqual(rows[0]['postprocess_data']['proof size'],12)
            self.assertEqual(rows[0]['postprocess_return_code'],'124')
            self.assertEqual(rows[0]['postprocess_error'],'Postprocessor timed out')
            self.assertEqual(job.outcome_counts,{('ok',status):1})
            self.assertEqual(outcome_breakdown(job)[0]['label'],status)
            self.assertIn({'label':status,'source':'solver','count':1},history_outcomes(job))

    def test_legacy_and_malformed_data(self):
        fields=row();fields[15]='THM'
        result=parse_csv_results(csv_text(fields[:17]))[0]
        self.assertEqual(call_outcome(result),('Theorem','solver'))
        for data in ['{bad','[]','null','42','"status"']:
            fields=row();fields[17]=data;fields[20]=''
            result=parse_csv_results(csv_text(fields))[0]
            self.assertEqual(result.postprocess_data,{})
            self.assertEqual(call_outcome(result),('OK','execution'))
        fields=row();fields[17]='{}'
        self.assertEqual(call_outcome(parse_csv_results(csv_text(fields))[0]),('sat','solver'))
        record=parse_jsonl_results(json.dumps({'task_id':0,'complete':True,'status':'ok','postprocess_data':{'status':'unsat'},'postprocess_error':'oops'}))[0]
        self.assertEqual(call_outcome(record),('unsat','solver'))

    def test_compact_headnode_rollup_preserves_raw_labels_and_legacy(self):
        program=re.search(r"summary_payload=\$\(awk -F, '(.*?)' \"\$\{csv_results\[@\]\}\"",REMOTE_SNAPSHOT_SCRIPT,re.S).group(1)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'batch.csv'
            fields=row('sat');legacy=row();legacy[0]='1';legacy[15]='THM';legacy=legacy[:17]
            custom='line\nbreak\t"comma,status"\\end\r\b\f'
            complex_row=row(custom);complex_row[0]='2'
            duplicate=row(custom);duplicate[0]='3'
            path.write_text(csv_text(fields)+csv_text(legacy)+csv_text(complex_row)+csv_text(duplicate))
            payload=subprocess.run(['awk','-F,',program,str(path)],text=True,capture_output=True,check=True).stdout
        job=JobSnapshot('job','/jobs/job',0)
        parse_summary(job,payload)
        self.assertEqual(job.outcome_counts,{('ok','sat'):1,('ok','Theorem'):1,('ok',custom):2})
        self.assertEqual(job.completed,4)
        self.assertEqual(job.status_counts,{'ok':4})
        self.assertEqual(job.wall_sum,6)
        self.assertIn({'label':'sat','source':'solver','count':1},history_outcomes(job))
        self.assertNotIn('python3 - "${csv_results[@]}"',REMOTE_SNAPSHOT_SCRIPT)

    def test_validation_endpoint_delegates(self):
        app=create_app();app.config['TESTING']=True
        client=app.test_client()
        with client.session_transaction() as session:session['csrf']='test'
        for extra, expected in [({}, []), ({'allowed_roots':['/tmp/imported']}, ['/tmp/imported'])]:
            with self.subTest(extra=extra), patch('slurmy_web.app.workflows.validate_postprocessor_command',return_value={'valid':True,'message':'ready'}) as validate:
                response=client.post('/api/validate-postprocessor',json={'command':'./parse','directory':'/tmp/parser',**extra},headers={'X-CSRF-Token':'test'})
                self.assertEqual(response.status_code,200)
                self.assertEqual(response.json,{'valid':True,'message':'ready'})
                validate.assert_called_once_with('./parse','/tmp/parser',allowed_roots=expected)

    def test_ui_renders_fields_as_text(self):
        source=(Path(__file__).parent.parent/'slurmy_web/static/app.js').read_text()
        self.assertIn('Object.entries(data)',source)
        self.assertIn("Postprocessing error",source)
        self.assertIn('document.createTextNode(cell',source)
        self.assertIn('taskResult(task)',source)

if __name__=='__main__':unittest.main()
