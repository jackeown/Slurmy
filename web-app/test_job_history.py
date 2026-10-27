"""Compact history breakdowns preserve outcomes, unfinished work and job identity."""
import csv
import io
from pathlib import Path
import re
import subprocess
import unittest

from slurmy_web.app import history_outcomes, summary
from slurmy_web.monitor import JobSnapshot, ResultRecord, parse_summary, REMOTE_SNAPSHOT_SCRIPT


class JobHistoryTests(unittest.TestCase):
    def job(self):
        return JobSnapshot('workflow_1793101588_123', '/jobs/job', 1793101588,
                           metadata=dict(task_count=10, workflow_directory='/workflows/workflow', job_name='workflow (1)'))

    def test_compact_counts_keep_solver_answers_and_unfinished_distinct(self):
        job = self.job()
        parse_summary(job, 'completed\t6\nstatus\tok\t4\nstatus\ttime-limit\t2\noutcome\tok\tTheorem\t4\noutcome\ttime-limit\tTimeout\t2\n')
        self.assertEqual(history_outcomes(job), [dict(label='Not finished', source='', count=4),
            dict(label='Theorem', source='solver', count=4), dict(label='Timeout', source='solver', count=2)])
        data = summary(job)
        self.assertEqual(data['name'], 'workflow (1)')
        self.assertEqual(data['id'], job.job_id)
        self.assertEqual(data['workflow_name'], 'workflow')

    def test_incomplete_attempts_remain_unfinished_not_complete_errors(self):
        job = self.job()
        job.results={0:ResultRecord(0,True,'solver-error'),1:ResultRecord(1,False,'interrupted')}
        job.completed=1
        outcomes=history_outcomes(job)
        self.assertEqual(sum(item['count'] for item in outcomes), 10)
        self.assertEqual(outcomes[0]['count'], 9)
        self.assertIn(dict(label='Solver error', source='execution', count=1),outcomes)

    def test_legacy_summary_and_missing_metadata(self):
        job=self.job();job.completed=3;job.status_counts={'ok':2,'time-limit':1}
        outcomes=history_outcomes(job)
        self.assertEqual(sum(item['count'] for item in outcomes),10)
        self.assertEqual(outcomes[0]['count'],7)
        self.assertEqual(history_outcomes(JobSnapshot('old','/jobs/old',0)),[])

    def test_real_awk_summary_retries_do_not_double_count(self):
        expression=re.search(r"summary_payload=\$\(awk -F, '(.*?)' \"\$\{csv_results\[@\]\}\"",REMOTE_SNAPSHOT_SCRIPT,re.S).group(1)
        output=io.StringIO();writer=csv.writer(output,quoting=csv.QUOTE_ALL)
        def row(task,status,complete,szs):
            fields=['']*17;fields[0:4]=[str(task),str(complete).lower(),status,'0'];fields[4]='1';fields[15]=szs;writer.writerow(fields)
        row(0,'solver-error',False,'');row(0,'ok',True,'Theorem');row(1,'solver-error',True,'Timeout');row(2,'interrupted',False,'')
        payload=subprocess.run(['awk','-F,',expression],input=output.getvalue(),capture_output=True,text=True,check=True).stdout
        job=self.job();parse_summary(job,payload)
        self.assertEqual(job.completed,2)
        self.assertEqual(job.outcome_counts,{('ok','Theorem'):1,('time-limit','Timeout'):1})
        self.assertEqual(history_outcomes(job)[0]['count'],8)

    def test_bar_segments_tooltips_and_unfinished_marker(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
function el(tag,text,cls){return {tag,textContent:text,className:cls,children:[],style:{},attributes:{},events:{},hidden:false,offsetWidth:100,offsetHeight:20,
 append(...nodes){this.children.push(...nodes);},setAttribute(key,value){this.attributes[key]=value;},addEventListener(key,fn){this.events[key]=fn;},classList:{add(){}},getBoundingClientRect(){return {left:20,bottom:30};}};}
const context={el,badge:value=>el('span',value),outcomeColors:{theorem:'green','not finished':'gray'},otherOutcomeColors:['orange'],document:{body:{append(){}}},innerWidth:1000,innerHeight:800};
vm.createContext(context);vm.runInContext(source.slice(source.indexOf('let jobStatusTooltip'),source.indexOf("window.addEventListener('scroll',()=>{if(jobStatusTooltip)")),context);
assert.equal(context.jobCompletionLabel({total:10,completed:10,percent:100,state:'DONE'}),'Complete · 10 solver-problem pairs finished');
assert.equal(context.jobCompletionLabel({total:10,completed:4,percent:40,state:'STOPPED'}),'Stopped · 4 / 10 solver-problem pairs finished (40.0%)');
assert.match(context.jobCompletionLabel({total:0,completed:0,percent:0,state:'SUBMITTED'}),/^Submitted.*Awaiting/);
const bar=context.jobStatusBar({total:10,completed:4,state:'STOPPED',outcomes:[{label:'Theorem',source:'solver',count:4},{label:'Not finished',source:'',count:6}]});
assert.match(bar.children[0].children[1].textContent,/4 \/ 10 finished.*6 unfinished/);
assert.equal(bar.children[1].children.length,2);
const complete=context.jobStatusBar({total:10,completed:10,state:'DONE',outcomes:[{label:'Theorem',count:10}]});
assert.equal(complete.children[0].children[1].textContent,'10 / 10 finished');
const theorem=bar.children[1].children[0],unfinished=bar.children[1].children[1];
assert.equal(theorem.style.width,'40%');assert.equal(unfinished.style.width,'60%');
theorem.events.pointermove({clientX:100,clientY:200});
assert.equal(vm.runInContext('jobStatusTooltip.textContent',context),'Theorem (solver): 4 solver-problem pairs (40.0%)');
unfinished.events.pointermove({clientX:100,clientY:200});
assert.equal(vm.runInContext('jobStatusTooltip.textContent',context),'Not finished: 6 solver-problem pairs (60.0%)');
unfinished.events.pointerleave();assert.equal(vm.runInContext('jobStatusTooltip.hidden',context),true);
'''
        source = Path(__file__).parent/'slurmy_web/static/app.js'
        subprocess.run(['node','-e',script,str(source)],check=True,capture_output=True,text=True)
