"""The UI must report the age of the displayed snapshot, not the HTTP reply."""
from pathlib import Path
import subprocess
import unittest


class RefreshFreshnessTests(unittest.TestCase):
    def test_saved_failed_and_fresh_snapshot_labels(self):
        script = Path(__file__).parent / 'slurmy_web/static/app.js'
        source = script.read_text()
        function = source.split('function refreshLabel(data){', 1)[1].split('\nasync function fetchJobs()', 1)[0]
        program = '''const assert=require('assert');let lastRefreshMeta={},lastLoadedAt=null;
function refreshLabel(data){''' + function + '''
const stamp=100;
const failed=refreshLabel({refresh:{updated:stamp,stale:true,error:'offline'}});
assert(failed.includes('Saved snapshot from'));
assert(failed.includes('may be out of date'));
assert(failed.includes('failed'));
assert(!failed.includes('Last refreshed'));
assert.equal(lastLoadedAt.getTime(),stamp*1000);
assert(refreshLabel({refresh:{updated:stamp,stale:true,refreshing:true}}).includes('refreshing in background'));
assert(refreshLabel({refresh:{updated:stamp,stale:false}}).startsWith('Last refreshed'));
'''
        subprocess.run(['node', '-e', program], check=True, capture_output=True, text=True)
        self.assertIn('async function poll(){\n await refresh();', source)
        self.assertIn('autoRefreshPercent>=100&&!lastRefreshMeta.stale', source)
