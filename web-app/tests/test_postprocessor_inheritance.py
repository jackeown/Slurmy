"""Inherited labels reflect current workflow settings without filling the inputs."""
import _bootstrap  # noqa: F401
from pathlib import Path
import re
import shutil
import subprocess
import unittest


@unittest.skipUnless(shutil.which('node'), 'Node is required')
class PostprocessorInheritanceTests(unittest.TestCase):
    def test_live_labels_blank_timeout_and_legacy_loading(self):
        source=(Path(__file__).parents[1]/'slurmy_web/static/builder.js').read_text()
        functions=source[source.index(' function positiveTimeout('):source.index(' function postprocessorData(')]
        script=r'''
const assert=require('assert');
const option={textContent:''};
const workflow={id:'workflow-postprocessing',mode:{value:'default'},timeout:{value:'30'}};
const card={id:'solver',mode:{value:'workflow',querySelector:()=>option,refreshShell(){}},timeout:{value:''}};
const $=()=>workflow,postField=(node,name)=>node[name];
'''+functions+r'''
inheritPostprocessorLabels(card);
assert.equal(option.textContent,'From workflow (Built-in SZS Parser)');
assert.equal(card.timeout.placeholder,'From workflow (30 seconds)');
assert.equal(card.timeout.value,'');assert(positiveTimeout(card));
workflow.mode.value='custom';workflow.timeout.value='45';inheritPostprocessorLabels(card);
assert.equal(option.textContent,'From workflow (Custom postprocessor)');
assert.equal(card.timeout.placeholder,'From workflow (45 seconds)');
for(const value of ['0','1.5','601','no']){card.timeout.value=value;assert(!positiveTimeout(card));}
card.timeout.value='7';assert(positiveTimeout(card));
initializePostprocessor(card,{postprocessor_mode_inherit:true,postprocessor_timeout_inherit:false,postprocessor_override:true});
assert.equal(card.mode.value,'workflow');assert.equal(card.timeout.value,'7');
initializePostprocessor(card,{postprocessor_override:false});assert.equal(card.timeout.value,'');
workflow.timeout.value='';assert(!positiveTimeout(workflow));
'''
        result=subprocess.run(['node','-e',script],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_step_validation_accepts_blank_solver_timeout_but_checks_explicit_values(self):
        static=Path(__file__).parents[1]/'slurmy_web'
        template=(static/'templates/new.html').read_text()
        tag=re.search(r'<input name="postprocessor_timeout"[^>]*>',template).group()
        self.assertIn('data-optional="true"',tag)
        self.assertNotIn('required',tag)
        workflow_tag=re.search(r'<input name="workflow_postprocessor_timeout"[^>]*>',template).group()
        self.assertIn('required',workflow_tag)
        source=(static/'static/builder.js').read_text()
        validation=source[source.index('  const inputs=[...sections[step].querySelectorAll'):source.index('  for(const input of inputs.filter')]
        script=r'''const assert=require('assert');
const input={value:'',type:'number',dataset:{optional:'true'},closest:()=>null,focus(){},checkValidity(){return this.value===''||(/^[1-9][0-9]*$/.test(this.value)&&Number(this.value)<=600);},reportValidity(){}};
const step=0,sections=[{querySelectorAll:()=>[input]}];
function validate(){'''+validation+r'''}
validate();input.value='30';validate();
for(const value of ['0','1.5','601','bad']){input.value=value;assert.throws(validate,/correct the highlighted field/);}
input.value='';validate();
// Workflow-level timeout remains mandatory.
input.dataset={};assert.throws(validate,/Fill in each visible setting/);
'''
        result=subprocess.run(['node','-e',script],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
