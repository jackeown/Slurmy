"""Section transitions preserve state when interrupted and respect reduced motion."""
from pathlib import Path
import subprocess
import unittest


class SectionAnimationTests(unittest.TestCase):
    def test_animation_completion_reversal_details_and_reduced_motion(self):
        source=Path(__file__).parent/'slurmy_web/static/sections.js'
        script=r'''
const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
let reduced=false;const root={},animations=[];
const context={window:{},document:{documentElement:root,addEventListener(){}},matchMedia:()=>({matches:reduced}),
 getComputedStyle:node=>node===root?{getPropertyValue:key=>({'--section-duration':'320ms','--section-easing':'ease','--section-style':'slide-fade'})[key]}:{opacity:'.6',transform:'none'}};
function section(native=false){
 const header={},child={animate:animate},classes=new Set();
 const el={tagName:native?'DETAILS':'SECTION',open:true,children:[header,child],style:{height:'',overflow:'',boxSizing:''},dataset:{},
  classList:{contains:key=>classes.has(key),toggle(key,value){if(value)classes.add(key);else classes.delete(key);}},querySelector:()=>header,
  getBoundingClientRect(){return {height:el.painted??(native?!el.open:classes.has('collapsed'))?60:260};},animate};return el;
}
function animate(frames,options){let resolve;const a={frames,options,finished:new Promise(r=>resolve=r),resolve:()=>resolve(),cancel(){this.cancelled=true;}};animations.push(a);return a;}
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);
const api=context.window.SlurmySections;
(async()=>{
 const el=section();api.toggle(el,true);
 assert.equal(api.isCollapsed(el),true);assert.equal(el.classList.contains('collapsed'),false);
 assert.equal(animations[0].frames[0].height,'260px');assert.equal(animations[0].frames[1].height,'60px');assert.equal(el.style.overflow,'clip');
 animations[0].resolve();await Promise.resolve();assert.equal(el.classList.contains('collapsed'),true);assert.equal(el.style.overflow,'');
 api.toggle(el,false);const opening=animations.at(-2);api.toggle(el,true);opening.resolve();await Promise.resolve();assert.equal(api.isCollapsed(el),true);
 animations.at(-2).resolve();await Promise.resolve();assert.equal(el.classList.contains('collapsed'),true);
 const details=section(true);api.toggle(details,true);assert.equal(details.open,true);animations.at(-2).resolve();await Promise.resolve();assert.equal(details.open,false);
 reduced=true;const before=animations.length;api.toggle(details,false);assert.equal(details.open,true);assert.equal(animations.length,before);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        result=subprocess.run(['node','-e',script,str(source)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
