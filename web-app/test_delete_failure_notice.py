"""SSH warnings stay in the progress log rather than flooding the failure notice."""
from pathlib import Path
import subprocess
import unittest


class DeleteFailureNoticeTests(unittest.TestCase):
    def test_failed_operation_uses_a_concise_confirmation_message(self):
        source=(Path(__file__).parent/'slurmy_web/static/app.js').read_text()
        function=source[source.index('async function deleteJob('):source.index('\nasync function renameJob(')]
        code='''const assert=require('assert');let jobs=[],jobData={name:'Job'},operationHidden=false,message;
const host='host',params=()=>'',SlurmyDialog={choose:async()=> 'keep'};
const sessionStorage={removeItem(){}},watchOperation=()=>{},notice=value=>{message=value;};
const setTimeout=fn=>{fn();return 1;};
async function api(path,data){
 if(path.includes('/delete-options'))return {synced:false};
 if(path.includes('/api/operations/'))return {state:'failed',log:'WARNING: key exchange algorithm. bind: line editing not enabled. '.repeat(1000)};
 return {id:'operation',state:'running'};
}
''' + function + '''
(async()=>{
 const control={textContent:'Delete',disabled:false};
 await deleteJob('job',control);
 assert(message.includes('could not be confirmed'));
 assert(message.includes('progress log'));
 assert(message.length<200);
 assert(!message.includes('key exchange'));
 assert.equal(control.disabled,false);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        subprocess.run(['node','-e',code],capture_output=True,text=True,check=True)
