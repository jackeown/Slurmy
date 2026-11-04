"""Core choices follow live topology and never silently reduce saved requests."""
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from slurmy_web.app import create_app
from slurmy_web import workflows


class PlacementControlTests(unittest.TestCase):
    def test_core_range_and_legacy_values(self):
        source=(Path(__file__).parent/'slurmy_web/static/builder.js').read_text()
        controls=source[source.index(' function syncPlacement(card){'):source.index(' function updateSolverStatus(card){')]
        code=r'''
const assert=require('assert');let change;let state={topology:{sockets:2,cores_per_socket:32}};
const core={value:'12',dataset:{coreRequest:'12'},replaceChildren(...options){this.options=options;},setAttribute(){}};
const socket={value:'false'},node={value:'false'},status={},retry={};
const fields={'[name="cores"]':core,'[name="exclusive_cpu"]':socket,'.core-validation':status,'[data-core-retry]':retry};
const card={querySelector:key=>fields[key]};
const form={addEventListener:(name,fn)=>{if(name==='change')change=fn;}};
const el=(tag,text)=>({textContent:text,value:text});const ensureCoreTopology=()=>state;
function updateSolverStatus(card){syncPlacement(card);}
'''+controls+r'''
function event(target){target.closest=()=>card;change({target});}
syncPlacement(card);assert.equal(core.options.length,64);assert.equal(core.options[63].value,'64');assert.equal(core.value,'12');
node.name='exclusive_node';node.value='true';event(node);assert.equal(socket.value,'true');assert.equal(core.options.length,32);
core.dataset.coreRequest='48';syncPlacement(card);assert.equal(core.value,'');assert.equal(core.dataset.coreValid,'false');assert.equal(core.options.length,33);assert.equal(core.options[0].disabled,true);
socket.name='exclusive_cpu';socket.value='false';event(socket);assert.equal(core.value,'48');assert.equal(core.dataset.coreValid,'true');
core.name='cores';core.value='8';event(core);assert.equal(core.dataset.coreRequest,'8');
socket.value='true';core.dataset.coreRequest='all';event(socket);assert.equal(core.value,'32');assert.equal(core.dataset.coreRequest,'32');
state={loading:true};syncPlacement(card);assert.equal(core.disabled,true);assert.equal(core.dataset.coreValid,'false');
state={error:'SSH unavailable'};syncPlacement(card);assert.equal(retry.hidden,false);assert.equal(status.textContent,'SSH unavailable');
'''
        result=subprocess.run(['node','-e',code],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_topology_requests_are_shared_and_old_responses_ignored(self):
        source=(Path(__file__).parent/'slurmy_web/static/builder.js').read_text()
        helper=source[source.index(' function ensureCoreTopology(){'):source.index(' function syncPlacement(card){')]
        code=r'''
const assert=require('assert');let coreTopologyState=null,selectedHost='a',partition='CPU-amd',csrf='token';
const rootValue=name=>name==='host'?selectedHost:partition;
let timer,requests=[],updates=0;
const setTimeout=fn=>{timer=fn;return 1;},clearTimeout=()=>{};
const form={querySelectorAll:()=>[{id:1},{id:2}]};const updateSolverStatus=()=>updates++;
const fetch=(path,options)=>new Promise(resolve=>requests.push({path,options,resolve}));
'''+helper+r'''
(async()=>{
 const first=ensureCoreTopology();assert.equal(ensureCoreTopology(),first);const old=timer();assert.equal(requests.length,1);
 selectedHost='b';const second=ensureCoreTopology();const fresh=timer();assert.equal(requests.length,2);assert.equal(requests[0].options.signal.aborted,true);
 requests[1].resolve({ok:true,json:async()=>({sockets:2,cores_per_socket:64})});await fresh;
 requests[0].resolve({ok:true,json:async()=>({sockets:1,cores_per_socket:8})});await old;
 assert.equal(coreTopologyState,second);assert.equal(second.topology.cores_per_socket,64);assert.equal(updates,2);
 ensureCoreTopology();assert.equal(requests.length,2);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        result=subprocess.run(['node','-e',code],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_topology_endpoint_reports_errors_and_hardware(self):
        with patch.object(workflows,'migrate_user_runs'):
            client=create_app().test_client()
        with client.session_transaction() as session:session['csrf']='test'
        with patch('check_threads.dedicated_core_topology',return_value=(2,32)) as query:
            response=client.post('/api/partition-core-topology',json={'host':'datalab','partition':'CPU-amd'},headers={'X-CSRF-Token':'test'})
        self.assertEqual(response.json,{'sockets':2,'cores_per_socket':32});query.assert_called_once_with('datalab','CPU-amd')
        with patch('check_threads.dedicated_core_topology',side_effect=ValueError('Mixed topology')):
            response=client.post('/api/partition-core-topology',json={'partition':'CPU-amd'},headers={'X-CSRF-Token':'test'})
        self.assertEqual(response.status_code,400);self.assertIn('Mixed topology',response.json['error'])
