"""Finished pairs keep the node and actual SLURM attempt that produced their result."""
import _bootstrap  # noqa: F401
import csv
import io
from pathlib import Path
import subprocess
import unittest
from slurmy_web.app import task_rows
from slurmy_web.monitor import JobSnapshot, ResultRecord, TaskDefinition, SlurmRecord, parse_csv_results, parse_slurm_lines


class ExecutionPlacementTests(unittest.TestCase):
    def job(self,result):
        job=JobSnapshot('job','/jobs/job',0,metadata={'batch_size':100})
        job.results={0:result};job.task_batches={0:0}
        job.submission_offsets={'100':0,'200':0}
        job.slurm=[SlurmRecord('200_0','200','0','RUNNING','','','','/jobs/job','new-node','queue'),
                   SlurmRecord('100_0','100','0','COMPLETED','','','','/jobs/job','old-node','accounting',raw_id='111')]
        return job

    def test_saved_provenance_works_after_accounting_has_expired(self):
        job=self.job(ResultRecord(0,True,'ok',node='saved-node',slurm_id='100_0'))
        job.slurm=[]
        row=next(task_rows(job))
        self.assertEqual((row['node'],row['slurm_id']),('saved-node','100_0'))
        self.assertTrue(row['node_historical'])

    def test_legacy_archive_matches_actual_attempt_instead_of_running_retry(self):
        job=self.job(ResultRecord(0,True,'ok',archive='batch_0_task_000000000_abcdef_111_1795600000.tar.gz'))
        row=next(task_rows(job))
        self.assertEqual(row['node'],'old-node')
        self.assertEqual(row['slurm_id'],'111')
        job.slurm=job.slurm[:1];del job._execution_records;del job._batch_allocations
        row=next(task_rows(job))
        self.assertEqual(row['node'],'')
        self.assertEqual(row['slurm_id'],'111')

    def test_unknown_legacy_attempt_is_not_attributed_to_a_later_retry(self):
        job=self.job(ResultRecord(0,True,'ok',archive='older-format.tar.gz'))
        row=next(task_rows(job))
        self.assertEqual(row['node'],'')
        self.assertEqual(row['slurm_id'],'')

    def test_pair_that_never_started_does_not_inherit_a_finished_batch_node(self):
        job=self.job(ResultRecord(0,True,'ok'))
        job.results={};job.tasks={0:TaskDefinition(0)};job.slurm=job.slurm[1:]
        row=next(task_rows(job))
        self.assertEqual(row['node'],'')

    def test_accounting_parser_retains_raw_job_id_and_result_parser_accepts_both_schemas(self):
        record=parse_slurm_lines('111|100_0|job|COMPLETED|1|start|end|submit|/jobs/job|node-1|','accounting')[0]
        self.assertEqual(record.raw_id,'111')
        fields=['0','true','ok','0','1','1','','','','','12','','','abcdef','archive','','','{}','','','','node-1','100_0']
        stream=io.StringIO();csv.writer(stream).writerow(fields)
        result=parse_csv_results(stream.getvalue())[0]
        self.assertEqual((result.node,result.slurm_id),('node-1','100_0'))
        stream=io.StringIO();csv.writer(stream).writerow(fields[:21])
        self.assertEqual(parse_csv_results(stream.getvalue())[0].node,'')

    def test_finished_placement_is_shown_as_history_without_a_live_cluster_link(self):
        source=(Path(__file__).parents[1]/'slurmy_web/static/app.js').read_text()
        function=source[source.index('function taskPlacement('):source.index('async function fetchJob(')]
        script="const assert=require('assert');const link=()=>{throw Error('Historical placement must not link to running tasks')};"+function+"assert.equal(taskPlacement({finished:true,node:'node-1',slurm_id:'100_0'},'job'),'node-1 · 100_0');"
        subprocess.run(['node','-e',script],check=True,capture_output=True,text=True)
