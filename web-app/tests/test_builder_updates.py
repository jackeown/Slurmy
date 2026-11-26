"""Workflow defaults, copying source globs, and remote axiom validation."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import json
import shutil
import subprocess
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from slurmy_web import workflows
from slurmy_web.app import create_app
from slurmy_web import batch_suggestion as batch

class BuilderUpdateTests(unittest.TestCase):
    def setUp(self):
        policy=patch.object(batch,'wall_time_policy',return_value={'limit_seconds':21600,'account':'default','partition_time_override':False})
        policy.start();self.addCleanup(policy.stop)
        tmp=tempfile.TemporaryDirectory();self.addCleanup(tmp.cleanup)
        self.root=Path(tmp.name);self.workflow=self.root/'sample';self.workflow.mkdir()
        (self.workflow/'Makefile').write_text('')
        for name,value in [('USER_RUNS',self.root),('BASE',self.root)]:
            p=patch.object(workflows,name,value);p.start();self.addCleanup(p.stop)
        with patch.object(workflows,'migrate_user_runs'):self.client=create_app().test_client()
        with self.client.session_transaction() as session:session['csrf']='test'

    def test_new_editor_has_implicit_solver_mode_and_default_fifty(self):
        response=self.client.get('/new')
        self.assertEqual(response.status_code,200)
        self.assertIn(b'type="hidden" name="mode" value="interactive"',response.data)
        self.assertNotIn(b'How do you want to define',response.data)
        self.assertIn(b'name="max_outstanding" type="number" min="1" step="1" value="50"',response.data)
        self.assertNotIn(b'id="path-go"',response.data)
        self.assertIn(b'id="batch-explanation-dialog"',response.data)
        self.assertNotIn(b'name="axiom_mode"',response.data)
        self.assertNotIn(b'Custom axiom globs',response.data)
        self.assertIn(b'id="workflow-postprocessing"',response.data)
        self.assertIn(b'name="workflow_postprocessor_mode"',response.data)
        self.assertNotIn(b'name="postprocessor_override"',response.data)
        self.assertIn(b'value="workflow">From workflow (Built-in SZS Parser)',response.data)
        self.assertIn(b'placeholder="From workflow (30 seconds)"',response.data)
        self.assertIn(b'tptp://TPTP-v10.0.0/Axioms/**/*.ax',response.data)
        self.assertIn(b'Add another axiom source',response.data)

    def test_copy_problem_sources_preserves_uri_and_deduplicates(self):
        glob='tptp://TPTP-v10.0.0/Problems/SET/*.p'
        with patch.object(workflows,'decisions',return_value=dict(globs=[glob,glob,'/tmp/local/*.p'])):
            response=self.client.get('/api/problem-sources',query_string=dict(directory=str(self.workflow)))
        self.assertEqual(response.status_code,200)
        self.assertEqual(response.json['globs'],[glob,'/tmp/local/*.p'])

    def test_copy_problem_sources_keeps_distinct_filename_selections(self):
        glob='tptp://TPTP-v10.0.0/Problems/SET/*.p'
        with patch.object(workflows,'decisions',return_value=dict(globs=[glob,glob,glob],problem_selections=['A.p','B.p','A.p'])):
            response=self.client.get('/api/problem-sources',query_string=dict(directory=str(self.workflow)))
        self.assertEqual(response.json['globs'],[glob,glob])
        self.assertEqual(response.json['problem_selections'],[['A.p'],['B.p']])

    def test_cluster_axiom_validation_does_not_convert_uri_to_local_path(self):
        uri='tptp://TPTP-v9.2.1/Axioms/SET*.ax'
        with patch('slurmy_archives.expand_axiom_source',return_value=[(Path('tptp:/share/datasets/TPTP/TPTP-v9.2.1/Axioms/SET001.ax'),Path('SET001.ax'))]) as expand:
            response=self.client.post('/api/validate-path?host=datalab',json=dict(kind='glob',role='axiom',value=uri),headers={'X-CSRF-Token':'test'})
        self.assertEqual(response.status_code,200)
        self.assertEqual(response.json['path'],uri);self.assertEqual(response.json['count'],1)
        self.assertEqual(expand.call_args.args[:2],(uri,'datalab'))

    def test_copy_axiom_sources_preserves_sources_and_supports_implicit_cluster_defaults(self):
        glob='tptp://TPTP-v9.2.1/Axioms/SET*.ax'
        for sources,expected in [([glob,glob,'/tmp/local/*.ax'],[glob,'/tmp/local/*.ax']),
                                 ([],['tptp://TPTP-v10.0.0/Axioms/**/*.ax'])]:
            with self.subTest(sources=sources), patch.object(workflows,'decisions',return_value=dict(axiom_globs=sources)):
                response=self.client.get('/api/axiom-sources',query_string=dict(directory=str(self.workflow)))
            self.assertEqual(response.status_code,200)
            self.assertEqual(response.json['globs'],expected)

    def test_suggestion_resolves_workflow_postprocessor_before_budgeting(self):
        node=dict(sockets=2,cores_per_socket=32,threads_per_core=1)
        config=dict(cores=1,threads_per_core=1,wc_limit=100,postprocessor_override=False,
                    postprocessor_mode='default',postprocessor_timeout=30)
        with patch.object(batch,'partition_nodes',return_value=[node]),patch.object(workflows,'problem_glob',return_value=('pattern',['a'])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=[config],globs=['pattern'],
                postprocessor=dict(mode='custom',directory='/parser',command='python3 parse.py',timeout=600)))
        self.assertEqual(result['max_postprocessor_seconds'],605)
        self.assertEqual(result['max_pair_budget_seconds'],785)

    def test_unified_suggestion_uses_both_balancing_and_actual_batch_groups(self):
        node=dict(sockets=2,cores_per_socket=32,threads_per_core=1)
        config=dict(cores=1,threads_per_core=1,solver_name='A',command='./solver {{problem}}',cpu_limit=1,wc_limit=1,mem_limit='100MiB')
        nodes=batch.Topology([node]*10,active_tasks=10)
        with patch.object(batch,'partition_nodes',return_value=nodes),patch.object(workflows,'problem_glob',return_value=('pattern',[str(i) for i in range(1000)])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=[config,{**config,'solver_name':'B'}],globs=['pattern'],max_outstanding=50,pair_order='solver-major'))
        self.assertEqual(result['balance_batch_size'],40)
        self.assertEqual(result['fit_batch_size'],50)
        self.assertEqual(result['batch_size'],50)
        self.assertEqual(result['suggested_batches'],40)
        self.assertEqual(result['recommended_batch_size'],result['batch_size'])
        self.assertTrue(result['suggested_fits'])

    @unittest.skipUnless(shutil.which('node'), 'Node is needed for the file import regression check')
    def test_multiple_name_files_are_read_in_order_and_failures_keep_previous_names(self):
        source=(Path(__file__).parent.parent/'slurmy_web/static/builder.js').read_text()
        start=source.index(" form.addEventListener('change',async event=>{\n  if(!event.target.matches('.problem-names-mode")
        end=source.index("\n for(const card",start)
        handler=source[source.index(' function nameFileContents('):source.index(' function fill(')]+source[start:end]
        program=r'''
const assert=require('assert');let change,validated=0;
const form={addEventListener:(type,fn)=>change=fn};
const list={children:[],replaceChildren(){this.children=[]},append(child){this.children.push(child)}};
const document={createElement:()=>({children:[],append(...children){this.children.push(...children)},setAttribute(){},addEventListener(type,handler){this.click=handler}})};
const mode={value:'files',matches:()=>true,closest:()=>row},status={},panel={},namesPanel={},glob={};
const text={value:'old.p',dispatchEvent:()=>{}};
const picker={files:[{name:'one.txt',text:async()=>"\uFEFFB.p\nA.p"},{name:'two.txt',text:async()=>"/other/C.p\nrelative/D.p"}],matches:()=>true,setCustomValidity(value){this.error=value},closest:()=>row};
const row={_namesMode:'files',querySelector:selector=>({'.problem-name-file-list':list,'.problem-names-text':namesPanel,'.problem-names-mode':mode,'.problem-name-files':picker,'[name="problem_selection"]':text,'.problem-names-status':status,'.problem-names-files':panel,'[name="glob"]':glob}[selector])};
const beginWork=()=>()=>{};const validatePath=async()=>{validated++};
'''+handler+r'''
(async()=>{
await change({target:picker});
assert.equal(text.value,'B.p\nA.p\n/other/C.p\nrelative/D.p');assert.equal(validated,1);assert.equal(picker.error,'');
assert.equal(status.textContent,'2 name files selected.');
assert.equal(namesPanel.hidden,true);
assert.equal(list.children.length,2);
picker.files=[{name:'third.txt',text:async()=> 'E.p'}];await change({target:picker});
assert.equal(list.children.length,3);assert(text.value.endsWith('E.p'));
await list.children[1].children[1].click();assert.equal(list.children.length,2);
assert.equal(text.value,'B.p\nA.p\nE.p');
mode.value='disabled';await change({target:mode});assert.equal(text.value,'');assert.equal(panel.hidden,true);assert.equal(namesPanel.hidden,true);assert.equal(picker.error,'');
mode.value='names';await change({target:mode});assert.equal(namesPanel.hidden,false);assert.equal(panel.hidden,true);text.value='typed.p';
mode.value='files';await change({target:mode});assert.equal(text.value,'B.p\nA.p\nE.p');assert.equal(namesPanel.hidden,true);assert.equal(panel.hidden,false);

picker.files=[{name:'bad.txt',text:async()=>{throw Error('read failed')}}];
const previous=text.value;await change({target:picker});assert.equal(text.value,previous);assert(picker.error);assert.equal(validated,6);
mode.value='names';await change({target:mode});assert.equal(text.value,'typed.p');
text.value='saved.p';initializeProblemNames(row,{problem_selection_mode:'files'});assert.equal(mode.value,'files');assert.equal(namesPanel.hidden,true);assert.equal(panel.hidden,false);assert.equal(text.value,'saved.p');
})().catch(error=>{console.error(error);process.exitCode=1});
'''
        result=subprocess.run(['node','-e',program],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_compact_file_picker_keeps_validation_for_hidden_native_input(self):
        html=self.client.get('/new').text
        self.assertIn('class="secondary add-name-files"',html)
        self.assertIn('class="problem-name-files" multiple data-optional="true" hidden',html)
        source=(Path(__file__).parent.parent/'slurmy_web/static/builder.js').read_text()
        start=source.index(' async function validSection(){')
        prefix=source[start:source.index("  if(step===1",start)]+'}'
        script=r"""
const assert=require('assert');let step=2;
const mode={value:'files'},picker={validationMessage:'',_reading:false},text={value:''};
const row={querySelector:key=>({'.problem-names-mode':mode,'.problem-name-files':picker,'[name="problem_selection"]':text}[key])};
const document={querySelectorAll:()=>[row]};
"""+prefix+r"""
(async()=>{
 await assert.rejects(validSection(),/Add a non-empty file/);
 text.value='A.p';picker._reading=true;await assert.rejects(validSection(),/finish loading/);
 picker._reading=false;picker.validationMessage='Read failed';await assert.rejects(validSection(),/Read failed/);
 picker.validationMessage='';await validSection();
 mode.value='disabled';text.value='';await validSection();
})().catch(error=>{console.error(error);process.exitCode=1;});
"""
        result=subprocess.run(['node','-e',script],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_environment_validation_accepts_literal_values_and_rejects_errors(self):
        headers={'X-CSRF-Token':'test'}
        response=self.client.post('/api/validate-solver-environment',json={'environment':'X=Y Z=3\nHello = World THREADS={{cores}}'},headers=headers)
        self.assertEqual(response.status_code,200)
        self.assertEqual(response.json['count'],4)
        for value in ['bad-name=x','X=one X=two','X="unfinished','X={{unknown}}','X={{var_file}}','X={{cores']:
            response=self.client.post('/api/validate-solver-environment',json={'environment':value},headers=headers)
            self.assertEqual(response.status_code,400)
