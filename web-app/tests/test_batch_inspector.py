"""Inspect real saved membership, preserving order and limiting work to visible pairs."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import time
import unittest
from unittest.mock import Mock, patch
from slurmy_web import app as web
from slurmy_web.batches import batch_members, batch_summaries
from slurmy_web.monitor import ClusterSnapshot, JobSnapshot, TaskDefinition, ResultRecord


class BatchInspectorTests(unittest.TestCase):
    def job(self):
        job=JobSnapshot('example','/jobs/example',time.time(),metadata={
            'task_count':8,'batch_size':4,'batch_count':2,
            'planned_batch_tasks':[[0,2,4,6],[7,1,5,3]]})
        job.tasks={i:TaskDefinition(i,solver='A' if i%2==0 else 'B',problem=f'/problems/{i}.p') for i in range(8)}
        job.task_batches={i: i%2 for i in range(8)}
        job.results[0]=ResultRecord(0,True,'ok')
        job.results[2]=ResultRecord(2,False,'interrupted')
        return job

    def client(self,job,stale=False):
        snapshot=ClusterSnapshot('datalab','/home/test','example',time.time(),[job])
        cache=Mock();cache.get.return_value=snapshot;cache.peek.return_value=snapshot
        cache.status.return_value=dict(stale=stale,updated=snapshot.fetched_epoch)
        with patch.object(web,'SnapshotCache',return_value=cache),patch.object(web.workflows,'migrate_user_runs'):
            app=web.create_app()
        app.config['COLLECTOR']=Mock(side_effect=AssertionError('Inspector must reuse the snapshot'))
        return app.test_client()

    def test_exact_interleaved_membership_and_execution_order(self):
        job=self.job()
        groups,estimated=batch_members(job)
        self.assertEqual(groups,{0:[0,2,4,6],1:[7,1,5,3]})
        self.assertFalse(estimated)
        self.assertIs(batch_members(job),job._batch_members)
        summary=batch_summaries(job,{},0,25)
        self.assertEqual([(row['total'],row['finished'],row['solvers']) for row in summary['batches']],[(4,1,['A']),(4,0,['B'])])
        self.assertEqual(summary['batches'][0]['state'],'Unfinished')

    def test_mixed_solver_batch_and_queue_timestamps_are_preserved(self):
        from slurmy_web.monitor import parse_slurm_lines
        import slurm_api
        job=self.job();job.tasks[2]=TaskDefinition(2,solver='B',problem='/problems/2.p')
        record,=parse_slurm_lines('123_0|123|0|RUNNING|1:00|5:00|job|/jobs/example|node1|2026-11-19T10:00:00|2026-11-19T10:02:00','queue')
        job.slurm=[record]
        row=batch_summaries(job,{0:record},0,25)['batches'][0]
        self.assertEqual(row['solvers'],['A','B'])
        self.assertEqual(row['scheduled'],'2026-11-19T10:00:00')
        self.assertEqual(row['started'],'2026-11-19T10:02:00')
        self.assertEqual(row['node'],'node1')
        pending,=parse_slurm_lines('123_0|123|0|PENDING|0|5:00|job|/jobs/example|Priority|2026-11-19T10:00:00|2026-11-20T10:02:00','queue')
        row=batch_summaries(job,{0:pending},0,25)['batches'][0]
        self.assertIsNone(row['started'])
        legacy,=parse_slurm_lines('123_0|123|0|RUNNING|1:00|5:00|job|/jobs/example|node1','queue')
        # Accounting fills timestamps when an older queue snapshot lacks them.
        row=batch_summaries(job,{0:legacy},0,25)['batches'][0]
        self.assertEqual(row['started'],'2026-11-19T10:02:00')
        self.assertEqual(slurm_api.queue_timestamp(0),'Unknown')
        self.assertEqual(slurm_api.queue_timestamp(1),'1970-01-01T00:00:01+00:00')

    def test_detail_pagination_only_builds_requested_rows_and_preserves_stale_flag(self):
        client=self.client(self.job(),stale=True)
        with patch.object(web,'unfinished_call',wraps=web.unfinished_call) as diagnostics:
            response=client.get('/api/jobs/example/batches/1?page=1&per_page=2')
        self.assertEqual(response.status_code,200)
        self.assertEqual([row['id'] for row in response.json['tasks']],[5,3])
        self.assertEqual([row['position'] for row in response.json['tasks']],[3,4])
        self.assertEqual(response.json['total'],4)
        self.assertTrue(response.json['refresh']['stale'])
        self.assertEqual(diagnostics.call_count,2)

    def test_summaries_are_paginated_without_pair_diagnostics(self):
        client=self.client(self.job())
        with patch.object(web,'task_rows',side_effect=AssertionError('No rich rows for summaries')):
            response=client.get('/api/jobs/example/batches?page=1&per_page=1')
        self.assertEqual(response.status_code,200)
        self.assertEqual([row['id'] for row in response.json['batches']],[1])
        self.assertEqual(response.json['total'],2)
        self.assertFalse(response.json['membership_estimated'])

    def test_legacy_mapping_is_used_and_missing_mapping_is_labelled_estimated(self):
        job=self.job();job.metadata.pop('planned_batch_tasks')
        groups,estimated=batch_members(job)
        self.assertEqual(groups,{0:[0,2,4,6],1:[1,3,5,7]});self.assertFalse(estimated)
        job=self.job();job.metadata.pop('planned_batch_tasks');job.task_batches={}
        groups,estimated=batch_members(job)
        self.assertEqual(groups,{0:[0,1,2,3],1:[4,5,6,7]});self.assertTrue(estimated)

    def test_linked_batch_selects_correct_page_even_for_sparse_batch_ids(self):
        job=self.job();job.metadata.pop('planned_batch_tasks');job.task_batches={0:4,1:168,2:168}
        client=self.client(job)
        response=client.get('/api/jobs/example/batches?batch=168&per_page=1')
        self.assertEqual(response.status_code,200,response.json)
        self.assertEqual(response.json['batches'][0]['id'],168)
        self.assertGreater(response.json['page'],0)
        self.assertEqual(client.get('/api/jobs/example/batches?batch=9999').status_code,404)

    def test_custom_page_sizes_over_one_hundred_are_honored_for_batches_and_pairs(self):
        job=self.job()
        job.metadata['planned_batch_tasks']=[list(range(250))]+[[task] for task in range(250,450)]
        job.tasks={task:TaskDefinition(task,solver='A',problem=f'/problems/{task}.p') for task in range(450)}
        client=self.client(job)
        summaries=client.get('/api/jobs/example/batches?per_page=150').json
        self.assertEqual(len(summaries['batches']),150)
        pairs=client.get('/api/jobs/example/batches/0?per_page=150').json
        self.assertEqual(len(pairs['tasks']),150)
        second=client.get('/api/jobs/example/batches/0?per_page=150&page=1').json
        self.assertEqual(len(second['tasks']),100)
        self.assertEqual(second['tasks'][0]['position'],151)
        self.assertEqual(client.get('/api/jobs/example/batches?per_page=10001').status_code,400)

    def test_unknown_batch_and_invalid_page_are_rejected(self):
        client=self.client(self.job())
        self.assertEqual(client.get('/api/jobs/example/batches/99').status_code,404)
        for suffix in ('?page=-1','?per_page=0','?page=oops'):
            self.assertEqual(client.get('/api/jobs/example/batches'+suffix).status_code,400)

    def test_finished_requires_every_pair_to_be_complete(self):
        job=self.job()
        for task in [0,2,4,6]:job.results[task]=ResultRecord(task,True,'time-limit')
        self.assertEqual(batch_summaries(job,{},0,25)['batches'][0]['state'],'Finished')


if __name__=='__main__':unittest.main()
