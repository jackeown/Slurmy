"""Custom theme editing previews safely and commits only on explicit Save."""
from pathlib import Path
import subprocess
import unittest


class CustomThemeTests(unittest.TestCase):
    def test_editor_preview_save_discard_images_and_storage_failure(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const nodes={},store=new Map([['slurmy-theme','garden']]),callbacks={};let quota=false,active=null;
function make(tag='div'){
 const n={tag,children:[],dataset:{},style:{values:{},setProperty(key,value){this.values[key]=value;}},events:{},attributes:{},value:'',hidden:false,disabled:false,
 setAttribute(key,value){this.attributes[key]=value;},addEventListener(type,fn){this.events[type]=fn;},append(...children){this.children.push(...children);},replaceChildren(){this.children=[];},
 click(){},focus(){active=this;},matches(){return !!this.open;},
 showPopover(){this.open=true;this.events.beforetoggle?.({newState:'open'});},hidePopover(){this.events.beforetoggle?.({newState:'closed'});this.open=false;}};
 Object.defineProperty(n,'id',{set(value){this._id=value;nodes['#'+value]=this;},get(){return this._id;}});return n;
}
const get=key=>nodes[key]||=make();const root=make();
let exported=null;
const context={Blob:class{constructor(parts){exported=JSON.parse(parts[0]);}},URL:{createObjectURL(){return 'blob:test';},revokeObjectURL(){}},setTimeout(fn){fn();},window:{dispatchEvent(){},addEventListener(){}},CustomEvent:class{},
 document:{body:make(),documentElement:root,createElement:make,querySelector:key=>key==='dialog[open]'?null:get(key),addEventListener:(key,fn)=>callbacks[key]=fn,get activeElement(){return active;}},
 localStorage:{getItem:key=>store.get(key)||null,setItem(key,value){if(quota&&key==='slurmy-custom-theme')throw Error('quota');store.set(key,value);}},matchMedia:()=>({matches:false}),
 FileReader:class {readAsDataURL(){this.result='data:image/png;base64,aGVsbG8=';this.onload();}},Image:class {naturalWidth=100;naturalHeight=100;decode(){return Promise.resolve();}}};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);vm.runInContext(fs.readFileSync(process.argv[3],'utf8'),context);vm.runInContext(fs.readFileSync(process.argv[2],'utf8'),context);
const themes=context.window.SlurmyThemes,editor=context.window.SlurmyCustomTheme;
const change=(key,value)=>{const input=get('#custom-'+key);input.value=String(value);input.events.input();};
(async()=>{
 editor.open();assert.equal(editor.isOpen,true);assert.equal(root.dataset.colorTheme,'custom');
 change('radius',24);change('font','serif');change('opacity',62);change('shadowBlur',40);change('textShadowBlur',12);
 assert.equal(root.style.values['--theme-radius'],'24px');assert.match(root.style.values['--theme-font'],/Georgia/);
 assert.equal(root.style.values['--theme-opacity'],'62%');assert.equal(root.style.values['--theme-chrome-opacity'],'52%');assert.match(root.style.values['--theme-shadow'],/40px/);
 assert.equal(store.get('slurmy-theme'),'garden');get('#discard-custom-theme').events.click();assert.equal(root.dataset.colorTheme,'garden');assert.equal(editor.isOpen,false);
 editor.open();assert.equal(get('#custom-radius').value,10);change('radius',24);change('collapseDuration',420);change('collapseEasing','gentle');change('collapseStyle','fade');assert.equal(root.style.values['--section-duration'],'420ms');assert.equal(root.style.values['--section-style'],'fade');change('ink','#345678');change('ink-alpha',40);get('#save-custom-theme').events.click();
 assert.equal(root.style.values['--ink'],'#34567866');
 assert.equal(themes.normalize({accent:'rgba(12, 34, 56, 0.5)'}).accent,'#0c223880');
 assert.equal(themes.normalize({accent:'rgba(999, 0, 0, 0.5)'}).accent,'#297351');
 assert.equal(themes.normalize({accent:'#12345600'}).accent,'#12345600');
 assert.equal(store.get('slurmy-theme'),'custom');assert.equal(themes.library[0].name,'My Theme');assert.equal(themes.library[0].settings.ink,'#34567866');assert.equal(JSON.parse(store.get('slurmy-custom-theme')).radius,24);assert.equal(root.style.values['--theme-radius'],'24px');
 editor.open();assert.equal(Number(get('#custom-ink-alpha').value),40);change('radius',3);get('#close-custom-theme').events.click();assert.equal(root.style.values['--theme-radius'],'24px');
 editor.open();change('radius',5);quota=true;get('#save-custom-theme').events.click();assert.equal(editor.isOpen,true);assert.equal(get('#custom-theme-error').hidden,false);assert.equal(themes.custom.radius,24);
 get('#discard-custom-theme').events.click();assert.equal(root.style.values['--theme-radius'],'24px');quota=false;
 editor.open();const input=get('#custom-theme-image');input.files=[{type:'image/png',size:21*1024*1024}];await input.events.change();assert.equal(get('#custom-image-error').hidden,false);
 input.files=[{type:'image/png',size:100}];await input.events.change();assert.match(root.style.values['--custom-image'],/^url\("data:image\/png/);
 get('#save-custom-theme').events.click();assert.equal(JSON.parse(store.get('slurmy-custom-theme')).image,'data:image/png;base64,aGVsbG8=');
 editor.open();get('#clear-custom-image').events.click();assert.equal(root.style.values['--custom-image'],'none');get('#discard-custom-theme').events.click();assert.match(root.style.values['--custom-image'],/data:image/);
 editor.open();const uploader=get('#upload-custom-theme');
 uploader.files=[{size:100,text:async()=>JSON.stringify({format:'slurmy-theme',version:1,name:'Imported Glow',settings:{accent:'rgba(12, 34, 56, 0.5)',radius:20}})}];
 await uploader.events.change();assert.equal(root.style.values['--green'],'#0c223880');assert.equal(get('#custom-theme-name').value,'Imported Glow');
 get('#download-custom-theme').events.click();assert.equal(exported.name,'Imported Glow');assert.equal(exported.settings.accent,'#0c223880');assert.equal(exported.format,'slurmy-theme');
 assert.equal(themes.library.length,1);get('#save-custom-theme').events.click();assert.equal(themes.library.length,2);
 editor.open();uploader.files=[{size:10,text:async()=>'{bad'}];await uploader.events.change();assert.equal(get('#custom-theme-error').hidden,false);
 get('#discard-custom-theme').events.click();
 // Restore the first saved theme for the reload assertions below.
 editor.open();get('#custom-theme-saved').value='0';get('#custom-theme-saved').events.change();get('#save-custom-theme').events.click();
 const invalid=themes.normalize({radius:999,opacity:-1,image:'url(https://example.com/track.png)',fontFamily:'x; background: url(evil)',shadowBlur:'oops'});
 assert.equal(themes.normalize({hover:'spotlight'}).hover,undefined);assert.equal(invalid.radius,28);assert.equal(invalid.opacity,40);assert.equal(invalid.image,'');assert.equal(invalid.fontFamily,'');assert.equal(invalid.shadowBlur,18);
 const reloaded={...context,window:{},document:{...context.document,documentElement:make()}};
 vm.createContext(reloaded);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),reloaded);
 assert.equal(reloaded.window.SlurmyThemes.library.length,2);assert.equal(reloaded.window.SlurmyThemes.library[0].name,'My Theme');assert.equal(reloaded.window.SlurmyThemes.custom.collapseDuration,420);assert.equal(reloaded.window.SlurmyThemes.custom.collapseEasing,'gentle');assert.equal(reloaded.window.SlurmyThemes.custom.ink,'#34567866');assert.equal(reloaded.window.SlurmyThemes.selected,'custom');assert.equal(reloaded.window.SlurmyThemes.custom.radius,24);assert.match(reloaded.window.SlurmyThemes.custom.image,/data:image/);
})().catch(error=>{console.error(error);process.exitCode=1;});
'''
        root=Path(__file__).parent/'slurmy_web/static'
        result=subprocess.run(['node','-e',script,str(root/'theme.js'),str(root/'custom-theme.js'),str(root/'background-image.js')],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
