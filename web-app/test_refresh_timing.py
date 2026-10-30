"""Local cache checks must not inherit the forced-SSH refresh throttle."""
from pathlib import Path
import subprocess
import unittest

SOURCE = (Path(__file__).parent / 'slurmy_web/static/app.js').read_text()


class RefreshTimingTests(unittest.TestCase):
    def test_cached_page_changes_are_debounced_but_only_forced_refreshes_are_throttled(self):
        function = SOURCE[SOURCE.index('function scheduleRefresh(fresh=false){'):SOURCE.index('\nrestoreTableUrl();')]
        code = '''const assert=require('assert');let refreshInFlight=false,refreshPending=false,refreshTimer=null,hasLoadedRemote=true,lastForcedRefreshStarted=Date.now(),delay,refreshCalled;
const setTimeout=(fn,ms)=>{delay=ms;return 1;},refresh=fresh=>{refreshCalled=fresh;};
''' + function + '''
scheduleRefresh(false);assert.equal(delay,150);
refreshTimer=null;scheduleRefresh(true);assert(delay>9000&&delay<=10000);
refreshTimer=null;hasLoadedRemote=false;scheduleRefresh(false);assert.equal(delay,150);
'''
        subprocess.run(['node', '-e', code], check=True, capture_output=True, text=True)
        self.assertIn("if(fresh&&hasLoadedRemote&&(page==='job'||page==='problem')", SOURCE)

    def test_pending_initial_load_is_checked_promptly_without_forcing_ssh(self):
        function = SOURCE[SOURCE.index('async function poll(){'):SOURCE.index("}if(['jobs','job','workflow','problem']")+1]
        code = '''const assert=require('assert');let loadingSnapshot=true,page='job',autoRefreshPercent=0,lastRefreshMeta={},autoRefreshTotal=0,delay,calls=[];
async function refresh(fresh){calls.push(fresh);}
const setTimeout=(fn,ms)=>{delay=ms;};
''' + function + '''
(async()=>{
 await poll();assert.equal(delay,1000);assert.equal(calls[0],undefined);
 loadingSnapshot=false;await poll();assert.equal(delay,10000);
 autoRefreshTotal=78792;await poll();assert.equal(delay,30000);
 autoRefreshPercent=100;lastRefreshMeta.stale=true;await poll();assert.equal(delay,30000);
 delay=null;lastRefreshMeta.stale=false;await poll();assert.equal(delay,null);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        subprocess.run(['node', '-e', code], check=True, capture_output=True, text=True)
