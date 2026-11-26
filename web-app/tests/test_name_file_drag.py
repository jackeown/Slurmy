"""Dragging name files updates saved order, while cancellation preserves it."""
import _bootstrap  # noqa: F401
import pathlib
import shutil
import subprocess
import unittest


@unittest.skipUnless(shutil.which('node'), 'Node is required')
class NameFileDragTests(unittest.TestCase):
    def test_drag_and_cancel(self):
        source=(pathlib.Path(__file__).parents[1]/'slurmy_web/static/builder.js').read_text()
        helpers=source[source.index(' function nameFileContents('):source.index(' function initializeProblemNames(')]
        script=r'''
const assert=require('assert');
class Element {
 constructor(){this.children=[];this.events={};this.style={cssText:''};this.classList={add(){},remove(){}};}
 append(...nodes){for(const n of nodes){n.remove();n.parent=this;this.children.push(n);}}
 remove(){if(this.parent){this.parent.children.splice(this.parent.children.indexOf(this),1);this.parent=null;}}
 replaceChildren(){for(const n of [...this.children])n.remove();}
 before(n){n.remove();n.parent=this.parent;this.parent.children.splice(this.parent.children.indexOf(this),0,n);}
 after(n){n.remove();n.parent=this.parent;this.parent.children.splice(this.parent.children.indexOf(this)+1,0,n);}
 setAttribute(){} removeAttribute(){} addEventListener(k,f){this.events[k]=f;}
 setPointerCapture(){this.capture=true;} hasPointerCapture(){return this.capture;} releasePointerCapture(){this.capture=false;}
 showPopover(){this.visible=true;} hidePopover(){this.visible=false;}
 getBoundingClientRect(){if(this===list)return {left:0,right:500,top:0,bottom:100};let i=this.parent.children.indexOf(this);return {left:i*100,right:i*100+90,top:0,bottom:30,width:90,height:30};}
 querySelectorAll(){return this.children.filter(n=>n.className!=='name-file-placeholder');}
}
const list=new Element(),text={value:'',dispatchEvent(){}},picker={setCustomValidity(){}},status={};
const document={createElement:()=>new Element(),elementFromPoint:()=>null};
const row={_nameFiles:[{name:'A',content:'A.p'},{name:'B',content:'B.p'},{name:'C',content:'C.p'}],querySelector(s){return s==='.problem-name-file-list'?list:s==='[name="problem_selection"]'?text:s==='.problem-name-files'?picker:s==='.problem-names-status'?status:{};}};
async function validatePath(){}
'''+helpers+r'''
renderNameFiles(row);
const event=(x)=>({button:0,isPrimary:true,pointerId:1,clientX:x,clientY:10,target:{closest:()=>null}});
let item=list.children[0];item.events.pointerdown(event(10));item.events.pointermove(event(390));
assert(item.visible);assert.equal(item.style.left,'380px');
item.events.pointerup(event(390));
assert.deepEqual(row._nameFiles.map(f=>f.name),['B','C','A']);
assert.equal(text.value,'B.p\nC.p\nA.p');assert.equal(row._filesDraft,text.value);
item=list.children[0];item.events.pointerdown(event(10));item.events.pointermove(event(390));item.events.pointercancel();
assert(!item.visible);assert.deepEqual(row._nameFiles.map(f=>f.name),['B','C','A']);assert.equal(list.children.length,3);
// Removal must use the newly reordered index.
list.children[1].children[1].events.click();
assert.deepEqual(row._nameFiles.map(f=>f.name),['B','A']);
'''
        result=subprocess.run(['node','-e',script],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
