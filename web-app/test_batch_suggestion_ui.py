"""Exercise the real suggestion controller against delayed responses and edits."""
from pathlib import Path
import subprocess
import unittest


class BatchSuggestionUITests(unittest.TestCase):
    def test_manual_override_stale_response_and_failure(self):
        script = r'''
const fs=require('node:fs'), vm=require('node:vm'), assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const controller=source.slice(source.indexOf(' const batchInput='),source.indexOf(' function renderStep()'));
const fields={};
function field(name){return fields[name] ||= {value:'',hidden:false,listeners:{},addEventListener(type,fn){this.listeners[type]=fn;}};}
const form={querySelector:()=>field('batch'),addEventListener:()=>{}};
let resolve;
const context={form,initial:{},step:4,$:field,payload:()=>({}),updateStepStates:()=>{},clearTimeout,setTimeout,
 api:()=>new Promise(done=>resolve=done)};
vm.createContext(context);vm.runInContext(controller,context);
const result={batch_size:20,total_pairs:1280,maximum_parallelism:32,node_count:10,hardware_parallelism:80};
(async()=>{
 let pending=vm.runInContext('suggestBatchSize()',context);resolve(result);await pending;
 assert.equal(field('batch').value,20);
 field('batch').value='7';field('batch').listeners.input();
 pending=vm.runInContext('suggestBatchSize()',context);resolve(result);await pending;
 assert.equal(field('batch').value,'7');assert.equal(field('#use-batch-suggestion').hidden,false);
 field('#use-batch-suggestion').listeners.click();assert.equal(field('batch').value,20);
 pending=vm.runInContext('suggestBatchSize()',context);const stale=resolve;
 const next=vm.runInContext('suggestBatchSize()',context);resolve({...result,batch_size:30});await next;
 stale({...result,batch_size:99});await pending;assert.equal(field('batch').value,30);
 context.api=async()=>{throw Error('offline');};await vm.runInContext('suggestBatchSize()',context);
 assert.match(field('#batch-suggestion').textContent,/offline.*manually/);
 assert.equal(field('batch').value,30);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        source = Path(__file__).parent/'slurmy_web/static/builder.js'
        subprocess.run(['node', '-e', script, str(source)], check=True, capture_output=True, text=True)
