"""Every SZS status survives summaries, display, acronym search and filters."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import csv
import io
import re
import subprocess
import unittest
import szs
from slurmy_web.app import call_outcome, task_page
from slurmy_web.monitor import JobSnapshot, ResultRecord, REMOTE_SNAPSHOT_SCRIPT, parse_summary, normalize_szs_result


class SZSResultTests(unittest.TestCase):
    def test_awk_summary_supports_all_mnemonics_and_comma_containing_assumptions(self):
        expression=re.search(r"summary_payload=\$\(awk -F, '(.*?)' \"\$\{csv_results\[@\]\}\"",REMOTE_SNAPSHOT_SCRIPT,re.S).group(1)
        output=io.StringIO();writer=csv.writer(output,quoting=csv.QUOTE_ALL)
        values=list(szs.STATUSES.values())+['ASS(UNK,THM)']
        for task,value in enumerate(values):
            fields=['']*17;fields[:5]=[str(task),'true','ok','0','1'];fields[15]=value;writer.writerow(fields)
        payload=subprocess.run(['awk','-F,',expression],input=output.getvalue(),capture_output=True,text=True,check=True).stdout
        job=JobSnapshot('job','/jobs/job',0);parse_summary(job,payload)
        self.assertEqual(job.completed,83)
        for value in values:
            self.assertEqual(job.outcome_counts[(szs.execution_status(value),szs.canonical(value))],1)
        self.assertEqual(job.wall_sum,83)

    def test_names_and_codes_normalize_to_identical_ui_categories(self):
        for name,code in szs.STATUSES.items():
            result=ResultRecord(0,True,'ok',return_code='0',szs_status=code)
            normalize_szs_result(result)
            self.assertEqual(result.szs_status,name)
            self.assertEqual(result.status,szs.execution_status(name))
            self.assertEqual(call_outcome(result),(name,'solver'))
        self.assertEqual(call_outcome(ResultRecord(0,True,'solver-error',szs_status='INE')),('InputError','solver'))

    def test_acronym_search_and_status_filter_match_canonical_results(self):
        job=JobSnapshot('job','/jobs/job',0,metadata={'task_count':4})
        job.results={0:ResultRecord(0,True,'ok',szs_status='Theorem'),1:ResultRecord(1,True,'no-success',szs_status='GaveUp'),
                     2:ResultRecord(2,True,'ok',szs_status='ContradictoryAxioms'),3:ResultRecord(3,True,'time-limit',szs_status='CPUTimeout')}
        rows,count=task_page(job,0,100,'id','asc',query='thm')
        self.assertEqual(count,1);self.assertEqual(rows[0]['szs_code'],'THM')
        rows,count=task_page(job,0,100,'id','asc',outcomes={('GUP','solver')})
        self.assertEqual(count,1);self.assertEqual(rows[0]['id'],1)
        rows,count=task_page(job,0,100,'id','asc',query='cto')
        self.assertEqual(count,1);self.assertEqual(rows[0]['id'],3)
