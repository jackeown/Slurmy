"""Local array indices retain exact batch identity across resume and accounting."""
import json
import unittest

from slurmy_web.app import batch_allocations, unfinished_call
from slurmy_web.batches import batch_summaries
from slurmy_web.monitor import parse_snapshot


class ArrayBatchMappingTests(unittest.TestCase):
    def snapshot(self, kind='SUBMISSION_CSV', delimiter=','):
        rows = [
            ['slurm_job_id','offset','array_size','submitted_epoch','batch_id','array_task_id'],
            ['100','2','1','1','2',''],  # Legacy parent_0.
            ['200','3','1','2','3','0'],
            ['200','3','1','2','4','1'],
            ['300','6','1','3','6','0'],  # Resume gap: separate parent.
            ['400','3','1','4','3','0'],  # A later attempt for batch 3.
        ]
        ledger='\n'.join(delimiter.join(row) for row in rows)+'\n'
        queue='\n'.join(f'{parent}_{element}|{parent}|{element}|RUNNING|1|60|job|/jobs/job|node|submit|start'
                        for parent,element in [('200','1'),('300','0'),('400','0')])
        accounting='11|200_0|job|FAILED|1|old-start|end|old-submit|/jobs/job|node\n'
        tokens=['REMOTE_HOME','/jobs','DETAIL','job','JOB','job','1',
                'METADATA','job',json.dumps({'task_count':7,'batch_count':7,'planned_batch_tasks':[[i] for i in range(7)]}),
                kind,'job',ledger,'SQUEUE',queue,'SACCT',accounting]
        return parse_snapshot('test', ('\0'.join(tokens)+'\0').encode()).jobs[0]

    def test_mixed_rows_and_resume_attempts_keep_exact_mapping(self):
        for kind,delimiter in [('SUBMISSION_CSV',','),('SUBMISSION_TSV','\t')]:
            with self.subTest(kind=kind):
                job=self.snapshot(kind,delimiter)
                self.assertEqual(job.submission_offsets,{'100':2,'200':3,'300':6,'400':3})
                self.assertEqual(job.submitted_batches,{2,3,4,6})
                self.assertEqual(job.active_batch_ids,{3,4,6})
                allocations=batch_allocations(job)
                self.assertEqual(allocations[3].display_id,'400_0')
                self.assertEqual(allocations[4].display_id,'200_1')
                self.assertEqual(unfinished_call(job,4,None)[0],'batch starting')
                summaries=batch_summaries(job,allocations,0,25)['batches']
                self.assertEqual(summaries[4]['slurm_id'],'200_1')
                self.assertEqual(summaries[4]['started'],'start')
                self.assertEqual(summaries[5]['state'],'Not submitted')
                self.assertEqual(summaries[2]['state'],'Unfinished')

    def test_accounting_only_element_uses_group_offset(self):
        job=self.snapshot()
        job.slurm=[record for record in job.slurm if record.source=='accounting']
        allocations=batch_allocations(job)
        self.assertEqual(set(allocations),{3})
        row=batch_summaries(job,allocations,0,25)['batches'][3]
        self.assertEqual((row['state'],row['slurm_id'],row['scheduled']),('Failed','200_0','old-submit'))

    def test_latest_accounting_attempt_replaces_old_cancelled_batch(self):
        from dataclasses import replace
        job=self.snapshot()
        original=next(record for record in job.slurm if record.source=='accounting')
        newer=replace(original,display_id='400_0',array_job_id='400',state='COMPLETED')
        job.slurm=[original,newer]
        self.assertEqual(batch_allocations(job)[3].display_id,'400_0')
        self.assertEqual(batch_summaries(job,batch_allocations(job),0,25)['batches'][3]['slurm_id'],'400_0')
        self.assertEqual(unfinished_call(job,3,None)[0],'result missing')

    def test_original_five_column_ledger_still_maps_parent_zero(self):
        tokens=['REMOTE_HOME','/jobs','JOB','job','1','SUBMISSION_CSV','job',
                'slurm_job_id,offset,array_size,submitted_epoch,batch_id\n100,42,1,1,42\n',
                'SQUEUE','100_0|100|0|RUNNING|1|60|job|/jobs/job|node']
        job=parse_snapshot('test', ('\0'.join(tokens)+'\0').encode()).jobs[0]
        self.assertEqual(job.active_batch_ids,{42})
        self.assertEqual(job.submitted_batches,{42})
