"""Action icons preserve labels and track controls created or updated at runtime."""
from pathlib import Path
import subprocess
import unittest


class ActionIconTests(unittest.TestCase):
    def test_semantic_icons_and_repeated_label_updates(self):
        root=Path(__file__).parent/'slurmy_web/static'
        script=r'''
const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
const context={window:{},document:{addEventListener(){}},requestAnimationFrame(){}};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);
const api=context.window.SlurmyActionIcons;
for(const [label,icon] of [['Duplicate workflow 3d-download','copy'],['Rename workflow download','pen'],['Delete workflow copy','trash-can'],['↓ Download','download'],['More download options','chevron-down'],['Browse cluster TPTP','folder-open'],['Restore','clock-rotate-left']])assert.equal(api.iconFor(label),icon);
const text={nodeType:3,nodeValue:'↓ Download'},attributes={};
const control={nodeType:1,childNodes:[text],dataset:{},matches:selector=>selector==='button,a.button,label.button,a.back',closest:()=>null,
 get textContent(){return text.nodeValue;},getAttribute:key=>attributes[key],setAttribute(key,value){attributes[key]=value;},querySelectorAll:()=>[],querySelector:()=>null,classList:{toggle(){}}};
api.scan(control);assert.equal(text.nodeValue,'Download');assert.equal(control.dataset.actionIcon,'download');
text.nodeValue='Duplicate';api.scan(control);assert.equal(control.dataset.actionIcon,'copy');assert.equal(text.nodeValue,'Duplicate');
api.scan(control);assert.equal(text.nodeValue,'Duplicate');
text.nodeValue='Something else';api.scan(control);assert.equal(control.dataset.actionIcon,undefined);
'''
        result=subprocess.run(['node','-e',script,str(root/'action-icons.js')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual((root/'vendor/fontawesome/fa-solid-900.woff2').read_bytes()[:4],b'wOF2')
        self.assertIn('SIL', (root/'vendor/fontawesome/LICENSE.txt').read_text())
