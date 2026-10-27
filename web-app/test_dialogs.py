"""Async app dialogs validate input, serialize requests, and cancel safely."""
from pathlib import Path
import subprocess
import unittest


class DialogTests(unittest.TestCase):
    def test_dialog_lifecycle_and_remember_choice(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
function node(){return {hidden:false,value:'',checked:false,children:[],listeners:{},attrs:{},style:{},isConnected:true,
 classList:{toggle(){}},focus(){this.focused=true;},select(){this.selected=true;},setAttribute(k,v){this.attrs[k]=v;},removeAttribute(k){delete this.attrs[k];},
 addEventListener(type,fn,options){(this.listeners[type]||=[]).push({fn,once:options?.once});},
 removeEventListener(type,fn){this.listeners[type]=(this.listeners[type]||[]).filter(x=>x.fn!==fn);},
 emit(type,event={}){for(const record of [...this.listeners[type]||[]]){if(record.once)this.removeEventListener(type,record.fn);record.fn(event);}},
 append(child){this.children.push(child);},replaceChildren(){this.children=[];},querySelector(){return this.children[0];},
 getBoundingClientRect(){return {left:10,right:400,top:10,bottom:300};}};}
const nodes={};const get=key=>nodes[key]||=node();
const dialog=get('dialog');dialog.querySelector=get;dialog.showModal=()=>dialog.open=true;dialog.close=()=>{dialog.open=false;dialog.emit('close');};
const previous=node();const context={window:{},document:{querySelector:()=>dialog,createElement:node,activeElement:previous}};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);
const api=context.window.SlurmyDialog,flush=()=>new Promise(setImmediate);
const submit=()=>get('form').emit('submit',{preventDefault(){},submitter:{value:'accept'}});
(async()=>{
 const answer=api.text({title:'Rename',input:{label:'Name',value:'',validate:value=>value.includes(' ')?'No spaces.':''}});
 await flush();assert.equal(dialog.open,true);assert.equal(get('#app-dialog-input').selected,true);
 submit();assert.equal(dialog.open,true);assert.equal(get('#app-dialog-error').textContent,'Enter a name.');
 get('#app-dialog-input').value='has spaces';submit();assert.equal(get('#app-dialog-error').textContent,'No spaces.');
 get('#app-dialog-input').value=' valid ';submit();assert.equal(await answer,'valid');assert.equal(previous.focused,true);
 const cancelling=api.choose({title:'Delete?',remember:true,choices:[{value:'delete',label:'Delete'}]});
 await flush();get('#app-dialog-remember').checked=true;dialog.emit('cancel',{preventDefault(){}});
 assert.equal(await cancelling,null);
 const first=api.choose({title:'First',choices:[{value:'accept',label:'OK'}]});
 const second=api.choose({title:'Second',remember:true,choices:[{value:'accept',label:'OK'}]});
 await flush();assert.equal(get('#app-dialog-title').textContent,'First');submit();assert.equal(await first,'accept');
 await flush();assert.equal(get('#app-dialog-title').textContent,'Second');assert.equal(get('#app-dialog-remember').checked,false);
 get('#app-dialog-remember').checked=true;submit();const remembered=await second;assert.equal(remembered.value,'accept');assert.equal(remembered.remember,true);
 const backdrop=api.choose({title:'Outside',choices:[{value:'accept',label:'OK'}]});await flush();
 dialog.emit('click',{target:dialog,clientX:0,clientY:0});assert.equal(await backdrop,null);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        path = Path(__file__).parent/'slurmy_web/static/dialog.js'
        subprocess.run(['node','-e',script,str(path)],check=True,capture_output=True,text=True)

    def test_remembered_choice_expires_and_rejects_invalid_storage(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');let stored;
const context={localStorage:{getItem(){return stored;}}};vm.createContext(context);
vm.runInContext(source.slice(source.indexOf('function rememberedWorkflowDeletion('),source.indexOf('async function deleteWorkflow(')),context);
stored=JSON.stringify({mode:'permanent',expires:310000});assert.equal(context.rememberedWorkflowDeletion(10000),'permanent');
assert.equal(context.rememberedWorkflowDeletion(310000),null);
stored=JSON.stringify({mode:'archive',expires:310000});assert.equal(context.rememberedWorkflowDeletion(10000),'archive');
stored=JSON.stringify({mode:'permanent',expires:310001});assert.equal(context.rememberedWorkflowDeletion(10000),null);
stored='broken JSON';assert.equal(context.rememberedWorkflowDeletion(10000),null);
'''
        path = Path(__file__).parent/'slurmy_web/static/app.js'
        subprocess.run(['node','-e',script,str(path)],check=True,capture_output=True,text=True)

    def test_workflow_deletion_remembers_only_explicit_choice_and_cancel_does_nothing(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
let now=10000,stored=null,choice=null,questions=0;const calls=[];
const context={Date:{now:()=>now},localStorage:{getItem:()=>stored,setItem:(key,value)=>stored=value},
 SlurmyDialog:{choose:async()=>{questions++;return choice;}},api:async(url,data)=>calls.push(data),params:()=>'',location:{},notice:()=>{}};
vm.createContext(context);vm.runInContext(source.slice(source.indexOf('function rememberedWorkflowDeletion('),source.indexOf("$('#delete-workflow')")),context);
const control={disabled:false,focus(){}};
(async()=>{
 await context.deleteWorkflow('/workflow','workflow',control);
 assert.equal(calls.length,0);assert.equal(stored,null);assert.equal(control.disabled,false);
 choice={value:'archive',remember:false};await context.deleteWorkflow('/workflow','workflow',control);
 assert.equal(calls.at(-1).permanent,false);assert.equal(stored,null);
 choice={value:'permanent',remember:true};await context.deleteWorkflow('/workflow','workflow',control);
 assert.equal(calls.at(-1).permanent,true);assert.equal(JSON.parse(stored).expires,310000);
 const asked=questions;await context.deleteWorkflow('/other','other',control);assert.equal(questions,asked);assert.equal(calls.at(-1).permanent,true);
 now=310000;choice=null;const count=calls.length;await context.deleteWorkflow('/third','third',control);
 assert.equal(questions,asked+1);assert.equal(calls.length,count);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        path = Path(__file__).parent/'slurmy_web/static/app.js'
        subprocess.run(['node','-e',script,str(path)],check=True,capture_output=True,text=True)

    def test_no_native_popup_calls_in_app_code(self):
        import re
        root=Path(__file__).parent/'slurmy_web/static'
        for path in root.glob('*.js'):
            self.assertIsNone(re.search(r'\b(?:alert|confirm|prompt)\s*\(',path.read_text()),path.name)
