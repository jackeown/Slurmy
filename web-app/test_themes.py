"""Theme previews are temporary; saved palettes persist and retain readable text."""
from pathlib import Path
import subprocess
import unittest


class ThemeTests(unittest.TestCase):
    def test_palette_coverage_preview_commit_migration_and_contrast(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
function boot(stored=null,dark=false,blocked=false){
 const tokens={},root={dataset:{},style:{setProperty:(key,value)=>tokens[key]=value}};
 const storage={value:stored,writes:0,getItem(){if(blocked)throw Error('unavailable');return this.value;},setItem(key,value){if(blocked)throw Error('unavailable');this.value=value;this.writes++;}};
 const context={window:{},document:{documentElement:root,addEventListener(){}},localStorage:storage,matchMedia:()=>({matches:dark})};
 vm.createContext(context);vm.runInContext(source,context);return {api:context.window.SlurmyThemes,root,storage,tokens};
}
const {api,root,storage,tokens}=boot('light');
assert.equal(api.selected,'porcelain');assert.equal(root.dataset.theme,'light');assert.equal(storage.writes,0);
assert.equal(api.themes.length,16);
for(const [group,count] of [['Everyday Light',4],['Everyday Dark',4],['After Hours',4],['Patterned Light',4]])assert.equal(api.themes.filter(t=>t.group===group).length,count);
for(const theme of api.themes){
 api.apply(theme.id);assert.equal(root.dataset.colorTheme,theme.id);assert.equal(root.dataset.theme,theme.mode);assert.equal(tokens['--paper'],theme.colors[0]);assert.equal(storage.writes,0);
}
api.restore();assert.equal(root.dataset.colorTheme,'porcelain');
api.select('neon-haze');assert.equal(storage.value,'neon-haze');assert.equal(api.selected,'neon-haze');
api.apply('blueprint');api.restore();assert.equal(root.dataset.colorTheme,'neon-haze');
assert.equal(boot(storage.value).root.dataset.colorTheme,'neon-haze');
assert.equal(boot().api.selected,'porcelain');assert.equal(boot(null,true).api.selected,'evergreen');assert.equal(boot('garden',true).api.selected,'garden');
assert.equal(boot('dark').api.selected,'evergreen');assert.equal(boot('nonsense',true).api.selected,'evergreen');
assert.equal(boot(null,false,true).api.selected,'porcelain');
const luminance=hex=>{
 const rgb=hex.match(/[a-f0-9]{2}/gi).map(value=>parseInt(value,16)/255).map(value=>value<=.04045?value/12.92:((value+.055)/1.055)**2.4);
 return rgb[0]*.2126+rgb[1]*.7152+rgb[2]*.0722;
};
const contrast=(a,b)=>{const x=luminance(a),y=luminance(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);};
for(const theme of api.themes){
 for(const surface of theme.colors.slice(0,3)){
  assert.ok(contrast(theme.tokens['--ink'],surface)>=4.5,`${theme.name} text contrast`);
  assert.ok(contrast(theme.tokens['--muted'],surface)>=4.5,`${theme.name} secondary text contrast`);
 }
 assert.ok(contrast(theme.tokens['--accent-ink'],theme.tokens['--green'])>=4.5,`${theme.name} button contrast`);
}
'''
        path=Path(__file__).parent/'slurmy_web/static/theme.js'
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_hover_focus_dismiss_and_selection_in_picker(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const nodes={},events={};let active=null,writes=0,stored='evergreen';
function make(tag='div') {return {tag,children:[],dataset:{},style:{setProperty(){}},attributes:{},events:{},offsetWidth:420,offsetHeight:700,
 setAttribute(key,value){this.attributes[key]=value;},append(...children){this.children.push(...children);},addEventListener(type,fn){this.events[type]=fn;},
 focus(){active=this;this.events.focus?.();},getBoundingClientRect(){return {left:20,right:170,bottom:800};},
 matches(){return !!this.open;},showPopover(){this.open=true;this.events.beforetoggle?.({newState:'open'});},hidePopover(){this.events.beforetoggle?.({newState:'closed'});this.open=false;},contains(target){return this===target||this.children.some(child=>child.contains(target));}};}
const get=selector=>nodes[selector]||=make();const root=make();
const context={window:{addEventListener(){}},innerWidth:1400,innerHeight:900,queueMicrotask,
 document:{body:make(),documentElement:root,createElement:make,querySelector:get,addEventListener:(type,fn)=>events[type]=fn,get activeElement(){return active;}},
 localStorage:{getItem:()=>stored,setItem:(key,value)=>{stored=value;writes++;}},matchMedia:()=>({matches:true})};
vm.createContext(context);vm.runInContext(fs.readFileSync(process.argv[1],'utf8'),context);events.DOMContentLoaded();
const api=context.window.SlurmyThemes,trigger=get('#theme-toggle'),panel=get('#theme-picker');
const buttons=get('#theme-options').children.flatMap(section=>section.children.filter(child=>child.tag==='button'));
const option=id=>buttons.find(button=>button.dataset.themeId===id);
assert.equal(events.scroll,undefined,'Layout-driven scrolling must not dismiss the picker');
assert.equal(buttons.length,16);trigger.events.click({detail:1});assert.equal(panel.open,true);
option('neon-haze').events.pointerenter({pointerType:'mouse'});assert.equal(root.dataset.colorTheme,'neon-haze');assert.equal(writes,0);
panel.events.pointerleave();assert.equal(root.dataset.colorTheme,'evergreen');
option('blueprint').focus();assert.equal(root.dataset.colorTheme,'blueprint');assert.equal(api.selected,'evergreen');
panel.hidePopover();assert.equal(root.dataset.colorTheme,'evergreen');assert.equal(trigger.attributes['aria-expanded'],'false');
trigger.events.click({detail:1});option('neon-haze').events.click();assert.equal(stored,'neon-haze');assert.equal(panel.open,false);assert.equal(root.dataset.colorTheme,'neon-haze');
trigger.events.click({detail:0});assert.equal(active,option('neon-haze'));
option('neon-haze').events.keydown({key:'ArrowDown',preventDefault(){}});assert.equal(root.dataset.colorTheme,'aurora');assert.equal(writes,1);
get('#close-theme-picker').events.click();assert.equal(root.dataset.colorTheme,'neon-haze');
option('garden').events.pointerenter({pointerType:'touch'});assert.equal(root.dataset.colorTheme,'neon-haze');
'''
        path=Path(__file__).parent/'slurmy_web/static/theme.js'
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_escape_on_pages_without_a_submission_menu(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8');
const line=source.split('\n').find(line=>line.startsWith("document.addEventListener('keydown'")&&line.includes('submitMenu'));
let handler,closed=0,focused=0;
const context={document:{addEventListener:(type,fn)=>handler=fn},submitMenu:null,submitMenuToggle:null,closeSubmitMenu:()=>closed++};
vm.createContext(context);vm.runInContext(line,context);handler({key:'Escape'});assert.equal(closed,0);
context.submitMenu={hidden:false};context.submitMenuToggle={focus:()=>focused++};handler({key:'Escape'});
assert.equal(closed,1);assert.equal(focused,1);
'''
        path=Path(__file__).parent/'slurmy_web/static/app.js'
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)

    def test_error_pages_share_the_theme_picker_even_with_an_invalid_host(self):
        from unittest.mock import patch
        from slurmy_web.app import create_app
        with patch('slurmy_web.workflows.migrate_user_runs'):
            client=create_app().test_client()
        for path in ('/page-that-does-not-exist','/new?host=invalid%20alias'):
            response=client.get(path)
            self.assertIn(response.status_code,(400,404))
            html=response.get_data(as_text=True)
            self.assertIn('id="theme-picker"',html)
            self.assertIn('themes.css',html)
            self.assertIn('Could Not Open This Page',html)

    def test_cursor_spotlight_eases_across_surfaces_and_respects_reduced_motion(self):
        script = r'''
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(process.argv[1],'utf8'),events={},frames=new Map(),media={matches:false};let id=0,glow;
function node(){return {style:{values:{},setProperty(key,value){this.values[key]=value;}},setAttribute(){},classList:{add(){},remove(){}}};}
const target=node();target.closest=selector=>selector.startsWith('#theme-picker')?null:target;
const sidebar=node();sidebar.closest=selector=>selector.startsWith('#theme-picker')?null:sidebar;
const context={root:{dataset:{themeHover:'spotlight'}},hoverTuning:{hoverLag:160},matchMedia:()=>media,
 document:{createElement:node,body:{append(n){glow=n;}},addEventListener:(type,fn)=>events[type]=fn},window:{addEventListener(){}},
 requestAnimationFrame:fn=>{frames.set(++id,fn);return id;},cancelAnimationFrame:key=>frames.delete(key)};
vm.createContext(context);vm.runInContext(source.slice(source.indexOf('  function initPointerEffects()'),source.indexOf("  document.addEventListener('DOMContentLoaded'")),context);context.initPointerEffects();
function move(surface,x){events.pointermove({target:surface,pointerType:'mouse',clientX:x,clientY:50});}
function tick(time){const [key,fn]=frames.entries().next().value;frames.delete(key);fn(time);}
move(target,100);tick(16);assert.equal(parseFloat(glow.style.values['--glow-x']),100);
move(target,400);tick(32);const first=parseFloat(glow.style.values['--glow-x']);assert.ok(first>100&&first<200);
tick(48);assert.ok(parseFloat(glow.style.values['--glow-x'])>first);
move(sidebar,500);tick(64);assert.ok(parseFloat(glow.style.values['--glow-x'])<300);assert.equal(glow.style.values['--glow-opacity'],'1');
const background=node();let backgroundActive=false;background.classList={add(){backgroundActive=true;},remove(){}};background.closest=selector=>selector==='main'?background:null;
move(background,600);tick(80);assert.equal(backgroundActive,true);assert.equal(glow.style.values['--glow-opacity'],'1');
for(const effect of ['ribbon','haze']){context.root.dataset.themeHover=effect;move(background,650);tick(96);assert.equal(glow.style.values['--glow-opacity'],'1');}
media.matches=true;move(sidebar,500);assert.equal(glow.style.values['--glow-opacity'],'0');assert.equal(frames.size,0);
'''
        path=Path(__file__).parent/'slurmy_web/static/theme.js'
        result=subprocess.run(['node','-e',script,str(path)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
