"""Account discovery uses the SSH identity and never mistakes failures for no access."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import subprocess
from pathlib import Path
import shutil
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from slurmy_web import accounts


class AccountTests(unittest.TestCase):
    def setUp(self):
        accounts._cache.clear()

    def test_discovery_deduplicates_memberships_caches_per_host_and_prefers_non_default(self):
        result=SimpleNamespace(returncode=0,stdout='datalab|default|jack||\ndatalab|cpuopt|jack||\ndatalab|cpuopt|jack|CPU-amd|\n')
        with patch.object(accounts.subprocess,'run',return_value=result) as run:
            first=accounts.memberships('datalab')
            self.assertEqual(first['accounts'],['cpuopt','default'])
            self.assertEqual(first['preferred'],'cpuopt')
            self.assertFalse(first['cached'])
            self.assertTrue(accounts.memberships('datalab')['cached'])
            self.assertEqual(run.call_count,1)
            accounts.memberships('another-host')
            self.assertEqual(run.call_count,2)
            self.assertIn('$(id -un)',run.call_args.args[0][-1])

    def test_choices_preserve_accessible_settings_and_remap_unavailable_portable_preferences(self):
        self.assertEqual(accounts.choose(['default']),'default')
        self.assertEqual(accounts.choose(['default'],'cpuopt'),'default')
        self.assertEqual(accounts.choose(['other','default'],'cpuopt'),'other')
        self.assertEqual(accounts.choose(['cpuopt','default'],'default'),'default')
        self.assertEqual(accounts.choose(['cpuopt','default'],''),'cpuopt')
        self.assertEqual(accounts.choose(['cpuopt','default']),'cpuopt')
        self.assertEqual(accounts.choose(['another','cpuopt','default']),'default')
        self.assertEqual(accounts.choose(['research']),'research')
        self.assertEqual(accounts.choose(['beta','alpha']),'alpha')
        self.assertEqual(accounts.choose(['gamma','beta','alpha']),'alpha')

    @unittest.skipUnless(shutil.which('node'), 'Node is needed for the browser control regression check')
    def test_dropdown_hides_default_only_and_preserves_saved_choices(self):
        source=(Path(__file__).parent.parent/'slurmy_web/static/builder.js').read_text()
        function=source[source.index(' async function loadAccounts(){'):source.index(" accountRetry.addEventListener('click',loadAccounts);")]
        program="""
const assert=require('assert');
let editing=false,initial={},accountsReady=false;
const accountField={},accountSelect={dataset:{},value:'',replaceChildren(){},add(){},events:[],dispatchEvent(event){this.events.push(event.type);}},accountLoading={},accountStatus={},accountRetry={};
const Option=function(text,value){this.value=value;};
const params=()=>'',rootValue=()=> 'test-host',updateStepStates=()=>{};
let result={accounts:['cpuopt','default'],preferred:'cpuopt',cached:false};
const api=async()=>{if(result instanceof Error)throw result;return result;};
"""+function+"""
(async()=>{
 await loadAccounts(); assert.equal(accountSelect.value,'cpuopt');assert.equal(accountField.hidden,false);assert.deepEqual(accountSelect.events,['change']);
 result={accounts:['default'],preferred:'default',cached:false};
 await loadAccounts();assert.equal(accountSelect.value,'default');assert.equal(accountField.hidden,true);assert.equal(accountsReady,true);
 result={accounts:['research'],preferred:'research',cached:false};
 await loadAccounts();assert.equal(accountSelect.value,'research');assert.equal(accountField.hidden,true);
 result={accounts:['cpuopt','other','default'],preferred:'default',cached:false};accountSelect.dataset={};
 await loadAccounts();assert.equal(accountSelect.value,'default');assert.equal(accountField.hidden,false);
 editing=true;initial={account:'default'};accountSelect.dataset={};
 result={accounts:['cpuopt','default'],preferred:'cpuopt',cached:true};
 await loadAccounts();assert.equal(accountSelect.value,'default');assert.equal(accountField.hidden,false);
 initial.account='missing';accountSelect.dataset={};
 await loadAccounts();assert.equal(accountSelect.value,'cpuopt');assert(accountStatus.textContent.includes('unavailable'));
 result=new Error('Disconnected');await loadAccounts();assert.equal(accountsReady,false);assert.equal(accountRetry.hidden,false);assert(accountLoading.textContent.startsWith('Disconnected'));assert(accountLoading.textContent.includes('keep editing'));
})().catch(error=>{console.error(error);process.exitCode=1});
"""
        result=subprocess.run(['node','-e',program],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_concurrent_account_lookup_does_not_start_or_wait_on_another_ssh(self):
        accounts._inflight.add('datalab')
        try:
            with patch.object(accounts.subprocess,'run') as run,self.assertRaisesRegex(ValueError,'already in progress'):
                accounts.memberships('datalab')
            run.assert_not_called()
        finally:accounts._inflight.discard('datalab')

    @unittest.skipUnless(shutil.which('node'), 'Node is needed for the editor regression check')
    def test_account_lookup_does_not_block_editing_the_next_step(self):
        source=(Path(__file__).parent.parent/'slurmy_web/static/builder.js').read_text()
        function=source[source.index(' async function validSection(){'):source.index(' let navigating=false;')]
        script="const step=0;const sections=[{querySelectorAll:()=>[]}];\n"+function+"\nvalidSection().catch(error=>{console.error(error);process.exitCode=1});"
        result=subprocess.run(['node','-e',script],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_errors_are_not_cached_or_interpreted_as_default_only(self):
        for result in [SimpleNamespace(returncode=1,stdout=''),SimpleNamespace(returncode=0,stdout='')]:
            with patch.object(accounts.subprocess,'run',return_value=result),self.assertRaises(ValueError):
                accounts.memberships('datalab')
            self.assertEqual(accounts._cache,{})
        with patch.object(accounts.subprocess,'run',side_effect=subprocess.TimeoutExpired('ssh',20)),self.assertRaises(ValueError):
            accounts.memberships('datalab')
        with self.assertRaises(ValueError):accounts.memberships('-bad')
