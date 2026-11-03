"""Operation output follows its tail without interrupting earlier-log reading."""
from pathlib import Path
import subprocess
import unittest


class OperationScrollTests(unittest.TestCase):
    def test_only_follow_when_already_at_bottom(self):
        source=(Path(__file__).parent/'slurmy_web/static/app.js').read_text()
        helper=source[source.index('function updateOperationLog('):source.index('async function watchOperation(')]
        code='''const assert=require('assert');let height=600,content='old',writes=0;
const log={clientHeight:200,scrollTop:400,get scrollHeight(){return height;},
 get textContent(){return content;},set textContent(value){content=value;height+=100;this.scrollTop=0;writes++;}};
const $=()=>log;
'''+helper+'''
updateOperationLog('first');assert.equal(log.scrollTop,700);
log.scrollTop=100;updateOperationLog('second');assert.equal(log.scrollTop,100);
log.scrollTop=599;updateOperationLog('third');assert.equal(log.scrollTop,900);
updateOperationLog('third');assert.equal(writes,3);
height=100;log.scrollTop=0;updateOperationLog('fourth');assert.equal(log.scrollTop,200);
'''
        result=subprocess.run(['node','-e',code],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
