"""Regressions for generated-file navigation and just-submitted job dates."""
import _bootstrap  # noqa: F401
from pathlib import Path
import subprocess
import unittest
from jinja2 import Environment, FileSystemLoader

WEB = Path(__file__).resolve().parents[1] / 'slurmy_web'


class IssueUIFixTests(unittest.TestCase):
    def test_generated_files_have_descriptive_order_and_preserve_file_actions(self):
        env = Environment(loader=FileSystemLoader(WEB/'templates'))
        template = env.from_string((WEB/'templates/experiment.html').read_text().replace("{% extends 'base.html' %}", ''))
        names = ['workflow.json','jobpairs.csv','configurations.csv','problem-globs.txt','axiom-globs.txt','problem-selection.json','building.txt','resource_limiter_template.txt','Makefile']
        html = template.render(files=dict.fromkeys(names,'[]'), directory='/users/demo', base='/users/', name='demo', host='datalab',
                               url_for=lambda *args, **kwargs:'/endpoint')
        labels=['Solvers (<code>configurations.csv</code>)','Problems (<code>problem-globs.txt</code>)',
                'Axioms (<code>axiom-globs.txt</code>)','Problem selection (<code>problem-selection.json</code>)',
                'Job pairs (<code>jobpairs.csv</code>)','Resource limiter (<code>resource_limiter_template.txt</code>)',
                'Solver and limiter builds (<code>building.txt</code>)','Workflow (<code>workflow.json</code>)',
                '<code>Makefile</code> (for hardcore users)','Prepared batches (for SLURM monitoring)']
        positions=[html.index(label) for label in labels]
        self.assertEqual(positions,sorted(positions))
        for name in names:
            self.assertIn(f'data-file="{name}"', html)
            self.assertIn(f'download="{name}"', html)

    def test_pending_dates_use_seconds_and_migrate_old_browser_records(self):
        source=(WEB/'static/app.js').read_text()
        functions=source[source.index('function pendingJobs()'):source.index('async function deleteJob(')]
        functions+=source[source.index('function announceJob('):source.index('\n',source.index('function announceJob('))]
        program=r'''
const assert=require('assert');const now=1795600000000;Date.now=()=>now;
let stored=JSON.stringify([{id:'old',host:'datalab',created:now-1000},{id:'seconds',host:'datalab',created:now/1000-10},{id:'expired',host:'datalab',created:now-4000000}]);
const sessionStorage={getItem:()=>stored,setItem:(key,value)=>stored=value};
const host='datalab',document={body:{dataset:{directory:'/workflow'}},querySelector:()=>({textContent:'Workflow'})};
'''+functions+r'''
assert.deepEqual(pendingJobs().map(job=>job.id),['old','seconds']);
assert.equal(pendingJobs()[0].created,(now-1000)/1000);
announceJob('new','New');const job=pendingJobs().find(job=>job.id==='new');
assert.equal(job.created,now/1000);
assert.equal(new Date(job.created*1000).getFullYear(),2026);
'''
        result=subprocess.run(['node','-e',program],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
