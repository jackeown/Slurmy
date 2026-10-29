"""Paging computes rich diagnostics only for visible rows unless required by search."""
import time
import unittest
from unittest.mock import patch

from slurmy_web import app as views
from slurmy_web.monitor import JobSnapshot,TaskDefinition,ResultRecord,BatchProgress,SlurmRecord


class VisibleTaskPageTests(unittest.TestCase):
    def job(self,count=2000):
        job=JobSnapshot('example','/jobs/example',time.time(),metadata={'task_count':count,'batch_size':100})
        for index in range(count):
            job.tasks[index]=TaskDefinition(index,solver='Solver '+str(index%3),problem=f'/problems/name-{index%10}.p')
            if index%3==0:job.results[index]=ResultRecord(index,True,'ok',cpu_seconds=index/10,wall_seconds=index/5,max_memory_kib=index+1,szs_status='Theorem')
        job.progress[4]=BatchProgress(4,'running',401,time.time()-100,time.time(),0)
        job.slurm=[SlurmRecord('1_4','1','4','RUNNING','','','','/jobs/example','node1','queue')]
        return job

    def test_default_page_only_builds_visible_rows(self):
        job=self.job(10000)
        with patch.object(views,'unfinished_call',wraps=views.unfinished_call) as diagnostics:
            rows,matched=views.task_page(job,5,25,'id','asc')
        self.assertEqual(matched,10000)
        self.assertEqual([row['id'] for row in rows],list(range(125,150)))
        self.assertLessEqual(diagnostics.call_count,25)

    def test_lightweight_global_sorts_preserve_previous_ordering(self):
        job=self.job()
        for field in ('id','solver','problem','cpu','memory','wall'):
            for direction in ('asc','desc'):
                with self.subTest(field=field,direction=direction):
                    expected=views.sorted_call_rows(list(views.task_rows(job)),field,direction)[20:40]
                    actual,matched=views.task_page(job,1,20,field,direction)
                    self.assertEqual([row['id'] for row in actual],[row['id'] for row in expected])
                    self.assertEqual(matched,2000)

    def test_search_outcomes_and_problem_subsets_keep_matched_counts(self):
        job=self.job()
        expected=[row for row in views.task_rows(job) if 'name-3.p' in ' '.join(str(value) for value in row.values()).lower()]
        actual,matched=views.task_page(job,1,10,'id','asc',query='name-3.p')
        self.assertEqual(matched,len(expected));self.assertEqual([row['id'] for row in actual],[row['id'] for row in expected[10:20]])
        rows,matched=views.task_page(job,0,10,'id','asc',outcomes={('Theorem','solver')})
        self.assertEqual(matched,len(job.results));self.assertTrue(all(row['id']%3==0 for row in rows))
        ids=[task_id for task_id,definition in job.tasks.items() if definition.problem=='/problems/name-1.p']
        with patch.object(views,'unfinished_call',wraps=views.unfinished_call) as diagnostics:
            rows,matched=views.task_page(job,0,5,'id','asc',task_ids=ids)
        self.assertEqual(matched,len(ids));self.assertLessEqual(diagnostics.call_count,5)
