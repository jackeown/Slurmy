"""One status cell retains scheduling details for hover, touch, and keyboard users."""
from pathlib import Path
import subprocess
import unittest


class TaskStatusTooltipTests(unittest.TestCase):
    def test_tooltip_only_shows_current_pair_and_supports_keyboard(self):
        path=Path(__file__).parent/'slurmy_web/static/app.js'
        script=r'''
const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
function el(){return {style:{},hidden:true,attributes:{},events:{},offsetWidth:200,offsetHeight:80,classList:{add(){}},
 setAttribute(key,value){this.attributes[key]=value;},removeAttribute(key){delete this.attributes[key];},addEventListener(key,fn){this.events[key]=fn;},getBoundingClientRect(){return {left:100,bottom:100};}};}
const context={el,badge:state=>{const node=el();node.textContent=state;return node;},document:{body:{append(){}}},innerWidth:1000,innerHeight:800};
vm.createContext(context);vm.runInContext(source.slice(source.indexOf('let jobStatusTooltip'),source.indexOf('function jobCompletionLabel')),context);
const queued=context.taskStatus({state:'queued',finished:false,will_run:'Yes',reason:'Waiting for SLURM capacity.'});
assert.equal(queued.textContent,'queued');assert.equal(queued.tabIndex,0);
queued.events.focus();assert.match(vm.runInContext('jobStatusTooltip.textContent',context),/Will run: Yes/);assert.match(vm.runInContext('jobStatusTooltip.textContent',context),/Not finished/);
queued.events.keydown({key:'Escape'});assert.equal(vm.runInContext('jobStatusTooltip.hidden',context),true);
const done=context.taskStatus({state:'Theorem',finished:true});done.events.pointermove({clientX:200,clientY:300});
assert.match(vm.runInContext('jobStatusTooltip.textContent',context),/Finished solver-problem pair/);assert.doesNotMatch(vm.runInContext('jobStatusTooltip.textContent',context),/SLURM capacity|Will run/);
done.events.blur();assert.equal(vm.runInContext('jobStatusTooltip.hidden',context),true);
queued.events.click();assert.match(vm.runInContext('jobStatusTooltip.textContent',context),/SLURM capacity/);
'''
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_both_tables_drop_redundant_columns(self):
        root=Path(__file__).parent/'slurmy_web/templates'
        for name,count in [('job.html',9),('problem.html',7)]:
            text=(root/name).read_text()
            self.assertNotIn('data-sort="will_run"',text)
            self.assertNotIn('data-sort="reason"',text)
            self.assertIn(f'colspan="{count}"',text)
