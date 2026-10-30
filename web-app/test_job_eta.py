"""Naive ETA uses observed solver wall time, not time spent stopped or queued."""
from pathlib import Path
import subprocess
import unittest

from slurmy_web.app import naive_eta_seconds, summary
from slurmy_web.monitor import JobSnapshot, SlurmRecord


class JobEtaTests(unittest.TestCase):
    def job(self):
        job = JobSnapshot('job', '/jobs/job', 0, metadata={'task_count':100}, completed=10, wall_sum=100)
        job.slurm = [SlurmRecord(str(index), str(index), '0', 'RUNNING', '', '', '', '/jobs/job', 'node', 'queue') for index in (1, 2)]
        return job

    def test_average_wall_time_divided_by_running_batches(self):
        job = self.job()
        self.assertEqual(naive_eta_seconds(job), 450)
        self.assertEqual(summary(job)['eta_seconds'], 450)
        job.slurm.append(SlurmRecord('3', '3', '0', 'PENDING', '', '', '', '/jobs/job', '', 'queue'))
        self.assertEqual(naive_eta_seconds(job), 450)
        job.created_epoch = -1000000000  # Long stops and creation timestamps do not inflate ETA.
        self.assertEqual(naive_eta_seconds(job), 450)

    def test_no_estimate_without_progress_running_batches_or_valid_wall_time(self):
        for field, value in (('completed', 0), ('completed', 100), ('wall_sum', 0), ('wall_sum', float('nan')), ('slurm', [])):
            with self.subTest(field=field, value=value):
                job = self.job()
                setattr(job, field, value)
                self.assertIsNone(naive_eta_seconds(job))

    def test_ui_explains_assumptions_and_marks_cached_eta_as_saved(self):
        source = (Path(__file__).parent / 'slurmy_web/static/app.js').read_text()
        functions = source[source.index('function jobCompletionLabel(job){'):source.index('function jobStatusBar(job){')]
        code = r'''
const assert=require('assert');
class Element {
 constructor(text=''){this.text=text;this.children=[];this.attributes={};this.events={};}
 append(...nodes){this.children.push(...nodes);}
 replaceChildren(...nodes){this.children=nodes;}
 setAttribute(key,value){this.attributes[key]=value;}
 addEventListener(key,fn){this.events[key]=fn;}
 get textContent(){return this.text+this.children.map(node=>node.textContent).join('');}
}
const target=new Element(),$=()=>target;
const document={createTextNode:text=>new Element(text)};
const el=(tag,text='',cls='')=>Object.assign(new Element(text||''),{tag,className:cls});
''' + functions + r'''
const data={state:'RUNNING',total:100,completed:10,percent:10,eta_seconds:450,refresh:{stale:false}};
renderJobCompletion(data);
assert(target.textContent.includes('ETA ~8m remaining'));
const help=target.children.at(-1),button=help.children[0],tip=help.children[1];
assert.equal(button.textContent,'?');
assert.equal(button.attributes['aria-describedby'],tip.id);
assert(tip.textContent.includes('naive estimate'));
assert(tip.textContent.includes('uniformly distributed in difficulty'));
assert.equal(tip.attributes.role,'tooltip');
data.refresh.stale=true;
renderJobCompletion(data);
assert(target.textContent.startsWith('Saved status:'));
assert(target.textContent.includes('Saved ETA ~8m remaining'));
data.eta_seconds=null;
renderJobCompletion(data);
assert(target.textContent.includes('ETA estimating'));
data.state='DONE';data.completed=100;data.percent=100;
renderJobCompletion(data);
assert(!target.textContent.includes('ETA'));
assert.equal(etaDuration(90000),'1d 1h');
'''
        subprocess.run(['node', '-e', code], check=True, capture_output=True, text=True)
