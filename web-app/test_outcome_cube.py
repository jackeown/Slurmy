"""3D chart labels follow the nearest edges without changing status counts."""
from pathlib import Path
import subprocess
import unittest


class OutcomeCubeTests(unittest.TestCase):
    def test_nearest_edges_and_status_order(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const start=source.indexOf('function nearestAxisEdges('), end=source.indexOf('function update(items,');
const context={};vm.createContext(context);vm.runInContext(source.slice(start,end),context);
for(const x of [-10,10])for(const z of [-10,10]){
 const edges=context.nearestAxisEdges({x,z},4,6);
 assert.equal(edges.x,x<0?-.5:3.5);assert.equal(edges.z,z<0?-.5:5.5);
 assert.equal(edges.xSign,Math.sign(x));assert.equal(edges.zSign,Math.sign(z));
}
// A pan may translate the camera past a midpoint without changing its orbit angle.
assert.equal(context.nearestAxisEdges({x:1,z:2},4,6).xSign,-1);
assert.equal(context.nearestAxisEdges({x:2,z:3},4,6).xSign,1);
for(const labels of [['Not finished','Timeout','Theorem','Error'],['Theorem','Timeout','Not finished'],['Timeout','Not finished']]){
 const original=labels.map((label,i)=>({label,count:i+3}));
 const ordered=context.orderCubeStatuses(original);
 assert.deepEqual(original.map(x=>x.label),labels);
 assert.equal(ordered[0].label,'Not finished');
 assert.deepEqual([...ordered].sort((a,b)=>a.count-b.count),original);
}
const items=[{label:'Theorem',source:'solver',count:10},{label:'Not finished',count:2},{label:'Timeout',source:'limiter',count:3}];
const preferred=[context.cubeStatusKey(items[2]),context.cubeStatusKey(items[0]),context.cubeStatusKey(items[1])];
const reordered=context.orderCubeStatuses(items,preferred);
assert.equal(reordered[0],items[2]);assert.equal(reordered[1],items[0]);assert.equal(reordered[2],items[1]);
const refreshed=context.orderCubeStatuses([...items,{label:'Error',source:'solver',count:1}],preferred);
assert.equal(refreshed[0],items[2]);assert.equal(refreshed[3].label,'Error');
// Status identity includes its origin, even if two statuses share a display label.
assert.notEqual(context.cubeStatusKey({label:'Timeout',source:'solver'}),context.cubeStatusKey({label:'Timeout',source:'limiter'}));
vm.runInContext(`
const solvers=['a','b','c','d'],statuses=[1,2,3,4,5,6];
const camera={position:{x:-10,z:-10,clone(){return this;}}},chart={worldToLocal(value){return value;}};
const axisLabels=['solver','status','count','tick'].map(kind=>({kind,offset:kind==='tick'?0:.5,item:{position:{x:0,y:2,z:0},scale:{x:1}}}));
updateAxisLabels();
`,context);
assert.equal(vm.runInContext('axisLabels[0].item.position.z',context),-1);
assert.equal(vm.runInContext('axisLabels[1].item.position.x',context),-1);
assert.equal(vm.runInContext('axisLabels[2].item.position.y',context),2);
assert.equal(vm.runInContext('axisLabels[3].item.scale.x',context),-1);
vm.runInContext('camera.position.x=10;camera.position.z=10;updateAxisLabels()',context);
assert.equal(vm.runInContext('axisLabels[0].item.position.z',context),6);
assert.equal(vm.runInContext('axisLabels[1].item.position.x',context),4);
assert.equal(vm.runInContext('axisLabels[3].item.scale.x',context),1);
'''
        path = Path(__file__).parent/'slurmy_web/static/outcome-3d.js'
        subprocess.run(['node', '-e', script, str(path)], check=True, capture_output=True, text=True)

    def test_drag_moves_actual_item_and_cleans_up_on_drop_or_cancel(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const legend={children:[],getBoundingClientRect:()=>({left:0,right:500,top:0,bottom:100}),
 querySelectorAll(){return this.children.filter(node=>node.className==='outcome-legend-item');}};
function make(tag,label){
 const node={style:{cssText:''},dataset:{},listeners:{},className:'',setAttribute(){},append(){},
 addEventListener(type,fn){this.listeners[type]=fn;},setPointerCapture(){},hasPointerCapture(){return true;},releasePointerCapture(){},
 getBoundingClientRect(){return {left:0,top:0,width:100,height:30,right:100,bottom:30};},
 before(other){other.remove();legend.children.splice(legend.children.indexOf(this),0,other);},
 after(other){other.remove();legend.children.splice(legend.children.indexOf(this)+1,0,other);},
 remove(){const i=legend.children.indexOf(this);if(i>=0)legend.children.splice(i,1);}};
 node.classList={add(name){node.activeClass=name;},remove(){node.activeClass=null;}};return node;
}
const moves=[];
const context={make,legend,labelFor:x=>x.label,cubeStatusKey:x=>x.label,statuses:[1,2,3],
 document:{createTextNode:()=>({}),elementFromPoint:()=>null},moveLegendStatus:(key,to)=>moves.push([key,to])};
vm.createContext(context);
const start=source.indexOf("    const item = make('span'), swatch = make('i'), grip"),end=source.indexOf('    legend.append(item);',start);
vm.runInContext('function build(status,z){'+source.slice(start,end)+'return item;}',context);
const item=context.build({label:'Not finished',color:'red'},0);
const b=make(),c=make();b.className=c.className='outcome-legend-item';
b.getBoundingClientRect=()=>({left:110,right:210,top:0,bottom:30,width:100,height:30});
c.getBoundingClientRect=()=>({left:220,right:320,top:0,bottom:30,width:100,height:30});
legend.children=[item,b,c];
const event=(x,y=10)=>({button:0,isPrimary:true,pointerId:1,clientX:x,clientY:y});
item.listeners.pointerdown(event(5));item.listeners.pointermove(event(280,20));
assert.equal(item.style.left,'275px');assert.equal(item.style.top,'10px');
assert.equal(item.activeClass,'dragging');
assert.equal(legend.children[3].className,'legend-placeholder');
item.listeners.pointerup(event(280,20));
assert.deepEqual(moves,[['Not finished',2]]);assert.equal(legend.children.length,3);
assert.equal(item.activeClass,null);assert.equal(item.style.cssText,'');
assert.equal(item.listeners.keydown,undefined);
item.listeners.pointerdown(event(5));item.listeners.pointermove(event(180));item.listeners.pointercancel();
assert.equal(legend.children.length,3);assert.equal(moves.length,1);assert.equal(item.activeClass,null);
'''
        path = Path(__file__).parent/'slurmy_web/static/outcome-3d.js'
        subprocess.run(['node', '-e', script, str(path)], check=True, capture_output=True, text=True)
