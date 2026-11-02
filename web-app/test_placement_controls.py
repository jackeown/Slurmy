"""Exercise the actual placement controls' transitions without cluster access."""
import json
from pathlib import Path
import subprocess
import unittest


class PlacementControlTests(unittest.TestCase):
    def test_node_defaults_all_and_restoring_numeric_limit(self):
        script=(Path(__file__).parent/'slurmy_web/static/builder.js').read_text()
        controls=script.split(' function syncPlacement(card){',1)[1].split(' function updateSolverStatus(card){',1)[0]
        harness=r'''
const assert=require('assert');let change;
const all={};const core={value:'12',type:'number',dataset:{}};
const socket={value:'false'},node={value:'false'},hint={};
const choice={value:'number',querySelector:()=>all};
const fields={'[name="cores"]':core,'[name="exclusive_cpu"]':socket,'[data-core-choice]':choice,'[data-all-cores]':hint};
const card={querySelector:key=>fields[key]};
const form={addEventListener:(name,fn)=>change=fn};
function updateSolverStatus(){}
'''
        checks=r'''
function event(target){target.closest=()=>card;target.matches=selector=>target===choice;change({target});}
syncPlacement(card);assert.equal(all.disabled,true);
node.name='exclusive_node';node.value='true';event(node);assert.equal(socket.value,'true');assert.equal(all.disabled,false);
choice.value='all';event(choice);assert.equal(core.value,'all');assert.equal(core.hidden,true);
node.value='false';event(node);assert.equal(socket.value,'false');assert.equal(core.value,'12');assert.equal(core.hidden,false);
socket.name='exclusive_cpu';socket.value='true';event(socket);assert.equal(all.disabled,false);
core.type='text';core.value='all';syncPlacement(card);assert.equal(choice.value,'all');
choice.value='number';event(choice);assert.equal(core.value,'12');assert.equal(core.type,'number');
'''
        result=subprocess.run(['node','-e',harness+'function syncPlacement(card){'+controls+checks],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
