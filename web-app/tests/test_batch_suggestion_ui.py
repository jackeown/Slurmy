"""Exercise the real suggestion controller against delayed responses and edits."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
from pathlib import Path
import subprocess
import unittest


class BatchSuggestionUITests(unittest.TestCase):
    def test_explanation_highlights_units_and_explains_fractional_rounding(self):
        script=r'''const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const render=source.slice(source.indexOf(' function renderBatchExplanation('),source.indexOf(' async function suggestBatchSize('));
function el(tag,text='',className=''){return {tag,text,className,children:[],append(...nodes){this.children.push(...nodes);},replaceChildren(){this.children=[];},setAttribute(){}};}
const content=el('div');
const data={total_pairs:30,configuration_count:1,problem_count:30,hardware_parallelism:1280,node_count:20,configuration_parallelism:[1280],configuration_names:['QA Solver'],capacity_breakdown:[[{nodes:20,formula:'floor((2 CPU sockets/compute node × 32 physical cores/CPU socket) ÷ 1 physical core/batch)'}]],assumed_hardware_parallelism:640,assumed_parallelism:50,balance_batch_size:1,fit_batch_size:1,workflow_submit_limit:50,account_active_tasks:0,account_free_slots:50,submission_budget:50,minimum_batches:1,group_pair_counts:[30],batch_size:1,suggested_batches:30,suggested_fits:true};
const context={el,$:()=>content,data,rootValue:()=>'datalab'};vm.createContext(context);vm.runInContext(render+';renderBatchExplanation(data);',context);
const nodes=[];function walk(node){nodes.push(node);node.children.forEach(walk);}walk(content);
const text=node=>String(node.text||'')+node.children.map(text).join('');const output=text(content);
assert.match(output,/floor\(1,280 parallel batches ÷ 2\) = 640 assumed parallel batches/);
assert.match(output,/ceil\(30 solver-problem pairs ÷ 50 parallel batches\) = 1 pair\/batch/);
assert.match(output,/gives 0.6 pairs per batch/);assert.match(output,/ceil rounds UP/);
assert.match(output,/no empty batches are created/);assert.match(output,/20 compute nodes/);assert.match(output,/2 CPU sockets/);
assert(nodes.some(n=>n.className==='math-function'&&n.text==='floor'));
assert(nodes.some(n=>n.className==='math-number'&&n.text==='640'));
assert(nodes.some(n=>n.className==='math-operator'&&n.text==='÷'));
data.account_submit_limit=null;data.account='cpuopt';data.account_free_slots=null;
data.account_active_tasks=60;data.workflow_submit_limit=100;data.assumed_parallelism=100;data.submission_budget=100;
context.renderBatchExplanation(data);
const accountOutput=text(content);
assert.match(accountOutput,/No account submission cap was reported for cpuopt/);
assert.doesNotMatch(accountOutput,/50 account task slots/);
'''
        source=Path(__file__).parent.parent/'slurmy_web/static/builder.js'
        subprocess.run(['node','-e',script,str(source)],check=True,capture_output=True,text=True)

    def test_wall_warning_updates_for_manual_edits_and_account_limits(self):
        source=(Path(__file__).parent.parent/'slurmy_web/static/builder.js').read_text()
        function=source[source.index(' function updateBatchTimeWarning()'):source.index(' async function suggestBatchSize()')]
        script=r"""
const assert=require('assert');const warning={},batchInput={value:'250'};
let lastBatchPlan={wall_limit_seconds:21600,wall_limit_label:'6 hours',batch_startup_seconds:60,max_pair_budget_seconds:121,account:'cpuopt'};
const $=()=>warning;
"""+function+r"""
updateBatchTimeWarning();assert.equal(warning.hidden,false);assert.match(warning.textContent,/6 hours.*cpuopt/);assert.match(warning.textContent,/cap the requested time/);
batchInput.value='100';updateBatchTimeWarning();assert.equal(warning.hidden,true);
batchInput.value='250';lastBatchPlan.wall_limit_seconds=86400;lastBatchPlan.wall_limit_label='24 hours';updateBatchTimeWarning();assert.equal(warning.hidden,true);
lastBatchPlan.wall_limit_seconds=null;updateBatchTimeWarning();assert.equal(warning.hidden,true);
lastBatchPlan.wall_limit_seconds=21600;lastBatchPlan.wall_time_unknown=true;updateBatchTimeWarning();assert.equal(warning.hidden,false);
batchInput.value='';updateBatchTimeWarning();assert.equal(warning.hidden,true);
"""
        subprocess.run(['node','-e',script],check=True,capture_output=True,text=True)

    def test_manual_override_stale_response_and_failure(self):
        script = r'''
const fs=require('node:fs'), vm=require('node:vm'), assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const controller=source.slice(source.indexOf(' const batchInput='),source.indexOf(' function renderStep()'));
const fields={};
function field(name){return fields[name] ||= {value:'',hidden:false,listeners:{},append(){},replaceChildren(){},setAttribute(){},addEventListener(type,fn){this.listeners[type]=fn;}};}
const form={querySelector:()=>field('batch'),addEventListener:()=>{}};
let resolve;
const context={form,initial:{},step:4,rootValue:()=>'datalab',$:field,payload:()=>({}),updateStepStates:()=>{},clearTimeout,setTimeout,
 el:()=>({append(){},setAttribute(){}}),text:()=>{},
 api:()=>new Promise(done=>resolve=done)};
vm.createContext(context);vm.runInContext(controller,context);
const result={batch_size:20,total_pairs:1280,maximum_parallelism:32,assumed_hardware_parallelism:40,assumed_parallelism:32,node_count:10,hardware_parallelism:80,configuration_count:2,problem_count:640,configuration_names:['A','B'],configuration_parallelism:[80,80],balance_batch_size:20,fit_batch_size:26,workflow_submit_limit:50,account_active_tasks:0,account_free_slots:50,submission_budget:50,minimum_batches:2,suggested_batches:64,suggested_fits:false};
(async()=>{
 let pending=vm.runInContext('suggestBatchSize()',context);resolve(result);await pending;
 assert.equal(field('batch').value,'');
 assert.equal(field('#use-batch-suggestion').hidden,false);
 field('batch').value='7';field('batch').listeners.input();
 pending=vm.runInContext('suggestBatchSize()',context);resolve(result);await pending;
 assert.equal(field('batch').value,'7');assert.equal(field('#use-batch-suggestion').hidden,false);
 field('batch').value='';field('batch').listeners.input();
 assert.equal(field('batch').value,'');
 pending=vm.runInContext('suggestBatchSize()',context);resolve(result);await pending;
 assert.equal(field('batch').value,'');
 field('#use-batch-suggestion').listeners.click();assert.equal(field('batch').value,20);
 pending=vm.runInContext('suggestBatchSize()',context);const stale=resolve;
 const next=vm.runInContext('suggestBatchSize()',context);resolve({...result,batch_size:30});await next;
 stale({...result,batch_size:99});await pending;assert.equal(field('batch').value,20);
 field('#use-batch-suggestion').listeners.click();assert.equal(field('batch').value,30);
 context.api=async()=>{throw Error('offline');};await vm.runInContext('suggestBatchSize()',context);
 assert.match(field('#batch-suggestion').textContent,/offline.*manually/);
 assert.equal(field('batch').value,30);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        source = Path(__file__).parent.parent/'slurmy_web/static/builder.js'
        subprocess.run(['node', '-e', script, str(source)], check=True, capture_output=True, text=True)
