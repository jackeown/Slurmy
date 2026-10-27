'use strict';

window.SlurmyThemes = (() => {
  const fonts = {
    system: 'Inter, ui-sans-serif, system-ui, sans-serif',
    humanist: '"Trebuchet MS", "Segoe UI", sans-serif',
    serif: 'Georgia, "Times New Roman", serif',
    rounded: 'ui-rounded, "Arial Rounded MT Bold", "Trebuchet MS", sans-serif',
    mono: '"SFMono-Regular", Consolas, "Liberation Mono", monospace',
    compact: '"Arial Narrow", "Liberation Sans Narrow", Arial, sans-serif',
  };
  const defaults = {mode:'light',paper:'#f3f6f0',surface:'#ffffff',well:'#edf2e9',ink:'#23392e',muted:'#59695f',line:'#d8e2d5',accent:'#297351',sidebar:'#172f27',accentInk:'#ffffff',
    font:'system',fontFamily:'',radius:10,opacity:100,blur:0,hover:'none',hoverLag:120,hoverSoftness:18,hoverStrength:12,transition:160,motion:'none',pattern:'none',patternSize:32,
    shadowColor:'#000000',shadowY:3,shadowBlur:18,shadowOpacity:5,textShadowColor:'#297351',textShadowBlur:0,textShadowOpacity:30,
    image:'',imageName:'',imageFit:'cover',imageOpacity:35};
  const choices = {mode:['light','dark'],font:Object.keys(fonts),hover:['none','lift','spotlight','ribbon','haze','shimmer','ink'],motion:['none','drift','breathe'],pattern:['none','grid','dots','weave','diagonal','stars'],imageFit:['cover','contain','tile']};
  const limits = {radius:[0,28],opacity:[40,100],blur:[0,20],transition:[0,600],hoverLag:[0,300],hoverSoftness:[0,40],hoverStrength:[1,30],patternSize:[8,100],shadowY:[0,24],shadowBlur:[0,60],shadowOpacity:[0,60],textShadowBlur:[0,24],textShadowOpacity:[0,80],imageOpacity:[0,100]};
  function normalizeColor(value) {
    if(typeof value !== 'string')return null;
    if(/^#[a-f0-9]{6}([a-f0-9]{2})?$/i.test(value))return value.toLowerCase();
    const match=value.match(/^rgba?\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})(?:\s*,\s*(0|1|0?\.\d+))?\s*\)$/i);
    if(!match || match.slice(1,4).some(v=>Number(v)>255))return null;
    return '#'+match.slice(1,4).map(v=>Number(v).toString(16).padStart(2,'0')).join('')+(match[4]===undefined?'':Math.round(Number(match[4])*255).toString(16).padStart(2,'0'));
  }
  function normalize(settings = {}) {
    const result = {...defaults};
    for (const key of ['paper','surface','well','ink','muted','line','accent','sidebar','accentInk','shadowColor','textShadowColor'])
      if (normalizeColor(settings[key])) result[key] = normalizeColor(settings[key]);
    for (const [key, values] of Object.entries(choices)) if(values.includes(settings[key])) result[key] = settings[key];
    for (const [key, [low,high]] of Object.entries(limits)) if(settings[key] !== '' && Number.isFinite(Number(settings[key]))) result[key] = Math.min(high,Math.max(low,Number(settings[key])));
    if(typeof settings.fontFamily === 'string' && /^[a-z0-9 ,"'_-]{0,160}$/i.test(settings.fontFamily)) result.fontFamily = settings.fontFamily;
    if(typeof settings.image === 'string' && settings.image.length <= 2800000 && /^data:image\/(png|jpeg|webp|gif);base64,[a-z0-9+/=]+$/i.test(settings.image)) result.image = settings.image;
    if(typeof settings.imageName === 'string') result.imageName = settings.imageName.replace(/[\x00-\x1f\x7f]/g,'').slice(0,160);
    return result;
  }
  const alpha = (hex, percent) => hex.slice(0,7) + Math.round((hex.length===9?parseInt(hex.slice(7),16):255) * percent / 100).toString(16).padStart(2,'0');
  function tokensFor(settings) {
    return {'--paper':settings.paper,'--white':settings.surface,'--surface-2':settings.well,'--ink':settings.ink,'--muted':settings.muted,'--line':settings.line,'--green':settings.accent,'--dark':settings.sidebar,'--accent-ink':settings.accentInk,
      '--red':settings.mode === 'dark' ? '#ff9b98' : '#ab363e','--orange':settings.mode === 'dark' ? '#e9bf7c' : '#956020',
      '--theme-font':settings.fontFamily || fonts[settings.font],'--theme-radius':settings.radius+'px','--theme-control-radius':Math.round(settings.radius*.7)+'px','--theme-opacity':settings.opacity+'%','--theme-blur':settings.blur+'px','--theme-transition':settings.transition+'ms','--hover-softness':settings.hoverSoftness+'px','--hover-strength':(settings.hoverStrength*.5)+'%',
      '--theme-shadow':settings.shadowOpacity ? `0 ${settings.shadowY}px ${settings.shadowBlur}px ${alpha(settings.shadowColor,settings.shadowOpacity)}` : 'none',
      '--theme-text-shadow':settings.textShadowBlur ? `0 0 ${settings.textShadowBlur}px ${alpha(settings.textShadowColor,settings.textShadowOpacity)}` : 'none',
      '--custom-image':settings.image ? `url("${settings.image}")` : 'none','--custom-image-opacity':settings.imageOpacity/100,'--custom-image-fit':settings.imageFit === 'tile' ? 'auto' : settings.imageFit,
      '--custom-pattern-size':settings.patternSize+'px'};
  }
  const profiles = {
    garden:{radius:10},porcelain:{radius:4,font:'humanist',hover:'lift',transition:100},sandstone:{radius:14,font:'serif',shadowColor:'#795a38',shadowOpacity:9},pearl:{radius:20,font:'rounded',hover:'lift',transition:240},
    evergreen:{radius:10},graphite:{radius:3,font:'compact',hover:'ink',transition:90},midnight:{radius:8,hover:'shimmer',transition:220},obsidian:{radius:2,font:'mono',shadowBlur:4,shadowOpacity:35},
    'neon-haze':{pattern:'grid',radius:5,font:'mono',hover:'ribbon',opacity:88,blur:10,motion:'drift',shadowColor:'#30ddca',shadowBlur:26,shadowOpacity:15,textShadowColor:'#68f8e7',textShadowBlur:12,textShadowOpacity:40},
    aurora:{pattern:'diagonal',radius:18,font:'humanist',hover:'haze',hoverLag:160,hoverSoftness:26,hoverStrength:9,opacity:88,blur:12,motion:'drift',transition:280,shadowColor:'#54cda4',shadowOpacity:12},
    amethyst:{pattern:'stars',radius:22,font:'serif',hover:'haze',hoverLag:180,hoverSoftness:30,hoverStrength:9,transition:360,opacity:90,blur:8,motion:'breathe',textShadowColor:'#d8adff',textShadowBlur:8,textShadowOpacity:28,shadowColor:'#a767d8',shadowOpacity:16},
    ember:{pattern:'diagonal',radius:3,font:'compact',hover:'ink',motion:'breathe',shadowColor:'#ff843a',shadowBlur:24,shadowOpacity:12},
    blueprint:{radius:2,font:'mono',hover:'ink',pattern:'grid'},botanical:{radius:18,font:'humanist',hover:'lift',opacity:94,pattern:'dots',transition:240},rosewater:{radius:22,font:'serif',hover:'shimmer',pattern:'weave',shadowColor:'#a34f77',shadowOpacity:8},
    sunprint:{radius:14,font:'rounded',hover:'spotlight',opacity:94,pattern:'diagonal',motion:'drift',shadowColor:'#a86d23',shadowOpacity:10},
  };
  const theme = (id, name, mode, group, detail, colors) => {
    const [paper,surface,well,ink,muted,line,accent,sidebar,accentInk] = colors;
    const settings = normalize({...defaults,...profiles[id],mode,paper,surface,well,ink,muted,line,accent,sidebar,accentInk});
    return {id,name,mode,group,detail,colors,settings,tokens:tokensFor(settings)};
  };
  const themes = [
    theme('garden','Garden','light','Everyday Light','Soft sage & evergreen', ['#f3f6f0','#ffffff','#edf2e9','#23392e','#59695f','#d8e2d5','#297351','#172f27','#ffffff']),
    theme('porcelain','Porcelain','light','Everyday Light','Crisp white & cobalt', ['#f3f5f9','#ffffff','#edf1f7','#24324c','#59677e','#d8dfeb','#315fbd','#18253f','#ffffff']),
    theme('sandstone','Sandstone','light','Everyday Light','Warm neutrals & bronze', ['#f6f1e8','#fffcf6','#eee7db','#40342a','#71604e','#dfd2bf','#94602d','#342a24','#ffffff']),
    theme('pearl','Pearl','light','Everyday Light','Rounded lilac & soft gray', ['#f4f2f8','#ffffff','#ece8f3','#352a48','#675a7b','#d8cfe5','#7753a4','#292238','#ffffff']),
    theme('evergreen','Evergreen','dark','Everyday Dark','The familiar forest night', ['#101915','#18241f','#111c18','#e4eee9','#9baba3','#344740','#64c99b','#0b1411','#0b2118']),
    theme('graphite','Graphite','dark','Everyday Dark','Quiet charcoal & silver', ['#15171b','#202329','#191c21','#eceef2','#a5adb9','#3b414c','#b1c5df','#101217','#152033']),
    theme('midnight','Midnight','dark','Everyday Dark','Deep navy & clear blue', ['#0f1726','#19253a','#121e30','#e3edff','#a0b1cc','#324763','#82b9f4','#090f1c','#10243d']),
    theme('obsidian','Obsidian','dark','Everyday Dark','Precise black & warm silver', ['#111111','#202020','#181818','#eeeae3','#b7afa3','#42403a','#d9c39e','#090909','#2b2418']),
    theme('neon-haze','Neon Haze','dark','After Hours','Cyan bloom & violet fog', ['#090d19','#131a2b','#0d1423','#e2f8ff','#a3b9cf','#33445f','#68f8e7','#080b16','#09242a']),
    theme('aurora','Aurora','dark','After Hours','Polar light over deep teal', ['#081b22','#102b32','#0a2229','#def9ee','#95bdb5','#285150','#7de8bc','#06151c','#0a3026']),
    theme('amethyst','Amethyst','dark','After Hours','Violet velvet & stardust', ['#191126','#281c39','#20152e','#f2e8ff','#bca7d2','#503768','#d8adff','#100c1c','#32154d']),
    theme('ember','Ember','dark','After Hours','Copper light & volcanic stone', ['#1b1210','#2c1e19','#221713','#ffebdc','#c6a894','#604234','#ffb477','#120c0a','#42230c']),
    theme('blueprint','Blueprint','light','Patterned Light','Engineering paper & blue grids', ['#edf5fc','#fbfdff','#e3eef9','#1d3c58','#486782','#c4d9ec','#2366a1','#16354d','#ffffff']),
    theme('botanical','Botanical','light','Patterned Light','Leaf-green ink & dappled paper', ['#f2f4e8','#fcfdf5','#e9eddc','#30442e','#596a50','#ced9bd','#507937','#263e2e','#ffffff']),
    theme('rosewater','Rosewater','light','Patterned Light','Rose ink & woven blush', ['#fcf1f2','#fffafb','#f5e7ec','#523443','#7f586b','#e8cbd6','#a34f77','#402331','#ffffff']),
    theme('sunprint','Sunprint','light','Patterned Light','Golden paper & drifting lines', ['#fbf4e5','#fffdf7','#f3e9d4','#49371e','#756044','#e4d1ad','#8e6428','#392b1d','#ffffff']),
  ];
  const cleanName = name => typeof name==='string'?name.replace(/[\x00-\x1f\x7f]/g,'').trim().slice(0,80):'';
  let library=[];
  try{const raw=JSON.parse(localStorage.getItem('slurmy-theme-library')||'[]');if(Array.isArray(raw))library=raw.filter(x=>x&&cleanName(x.name)&&x.settings).map(x=>({name:cleanName(x.name),settings:normalize(x.settings)}));}catch{}
  let custom = normalize(), hasCustom = false;
  try { const raw = localStorage.getItem('slurmy-custom-theme'); if(raw){custom = normalize(JSON.parse(raw)); hasCustom = true;} } catch {}
  const customTheme = settings => ({id:'custom',name:'Custom Theme',mode:settings.mode,settings,tokens:tokensFor(settings)});
  const root = document.documentElement;
  const find = id => id === 'custom' ? customTheme(custom) : themes.find(item => item.id === id);
  let stored;
  try { stored = localStorage.getItem('slurmy-theme'); } catch {}
  stored = stored === 'light' ? 'porcelain' : stored === 'dark' ? 'evergreen' : stored;
  let selected = find(stored)?.id || (matchMedia('(prefers-color-scheme: dark)').matches ? 'evergreen' : 'porcelain');

  let revision = 0, hoverTuning = {...defaults};
  function paint(item) {
    hoverTuning = item.settings;
    for (const [key,value] of Object.entries(item.tokens)) root.style.setProperty(key,String(value));
    root.dataset.theme = item.mode; root.dataset.colorTheme = item.id;
    root.dataset.themeHover = item.settings.hover; root.dataset.themeMotion = item.settings.motion;
    root.dataset.themePattern = item.id === 'custom' ? item.settings.pattern : 'preset';
    root.dataset.themeImageFit = item.settings.imageFit;
    root.dataset.themeRevision = String(++revision);
    root.style.colorScheme = item.mode;
  }
  function apply(id) { const item = find(id); if(item) paint(item); }
  function previewCustom(settings) { paint(customTheme(normalize(settings))); }
  function saveCustom(settings, name) {
    const next = normalize(settings);
    // Do not pretend to save if a large image exceeds the browser's quota.
    localStorage.setItem('slurmy-custom-theme',JSON.stringify(next));
    if(cleanName(name)){
      const entry={name:cleanName(name),settings:next};
      const updated=library.some(item=>item.name===entry.name)?library.map(item=>item.name===entry.name?entry:item):library.concat(entry);
      localStorage.setItem('slurmy-theme-library',JSON.stringify(updated));library=updated;
    }
    custom = next; hasCustom = true; select('custom');
  }
  function select(id) {
    if (!find(id)) return;
    selected = id;
    try { localStorage.setItem('slurmy-theme', id); } catch {}
    apply(id);
  }
  apply(selected);

  function initPicker() {
    const trigger = document.querySelector('#theme-toggle');
    const panel = document.querySelector('#theme-picker');
    const list = document.querySelector('#theme-options');
    const caption = document.querySelector('#theme-current-name');
    const status = document.querySelector('#theme-preview-status');
    if (!trigger || !panel) return;
    const buttons = [];
    const refreshSelection = () => {
      caption.textContent = find(selected).name;
      trigger.setAttribute('aria-label', `Color Theme: ${find(selected).name}`);
      for (const button of buttons) {
        button.setAttribute('aria-selected', String(button.dataset.themeId === selected));
        button.tabIndex = button.dataset.themeId === selected || (selected === 'custom' && button === buttons[0]) ? 0 : -1;
      }
    };
    const restore = () => { if(window.SlurmyCustomTheme?.isOpen)return; apply(selected); status.textContent = 'Hover to preview · click to keep'; };
    for (const group of [...new Set(themes.map(item => item.group))]) {
      const section = document.createElement('div'); section.className = 'theme-option-group';
      section.setAttribute('role','group'); section.setAttribute('aria-label',group);
      const heading = document.createElement('p'); heading.className = 'theme-group-title'; heading.textContent = group;
      section.append(heading);
      for (const item of themes.filter(item => item.group === group)) {
        const button = document.createElement('button'); button.type = 'button'; button.className = 'theme-option';
        button.dataset.themeId = item.id; button.setAttribute('role','option');
        const swatches = document.createElement('span'); swatches.className = 'theme-swatches'; swatches.setAttribute('aria-hidden','true');
        for (const color of [item.colors[0],item.colors[1],item.colors[6]]) {
          const dot = document.createElement('i'); dot.style.backgroundColor = color; swatches.append(dot);
        }
        const name = document.createElement('strong'); name.textContent = item.name;
        const detail = document.createElement('small'); detail.textContent = item.detail;
        const check = document.createElement('span'); check.className = 'theme-selected-check'; check.textContent = '✓'; check.setAttribute('aria-hidden','true');
        button.append(swatches,name,detail,check);
        const preview = () => { apply(item.id); status.textContent = `Previewing ${item.name} · click to keep`; };
        button.addEventListener('pointerenter',event => { if(event.pointerType !== 'touch') preview(); });
        button.addEventListener('focus',preview);
        button.addEventListener('click',() => { select(item.id); refreshSelection(); panel.hidePopover(); trigger.focus(); });
        button.addEventListener('keydown',event => {
          let index = buttons.indexOf(button);
          if (event.key === 'ArrowDown' || event.key === 'ArrowRight') index = (index + 1) % buttons.length;
          else if (event.key === 'ArrowUp' || event.key === 'ArrowLeft') index = (index - 1 + buttons.length) % buttons.length;
          else if (event.key === 'Home') index = 0;
          else if (event.key === 'End') index = buttons.length - 1;
          else return;
          event.preventDefault(); buttons[index].focus();
        });
        buttons.push(button); section.append(button);
      }
      list.append(section);
    }
    const position = () => {
      const bounds = trigger.getBoundingClientRect();
      const sidebar = document.querySelector('.sidebar')?.getBoundingClientRect();
      const left = innerWidth > 760 ? Math.max(bounds.right,sidebar?.right || bounds.right) + 12 : bounds.left;
      panel.style.left = `${Math.max(12,Math.min(left,innerWidth-panel.offsetWidth-12))}px`;
      const top = innerWidth > 760 ? bounds.bottom-panel.offsetHeight : bounds.bottom+10;
      panel.style.top = `${Math.max(12,Math.min(top,innerHeight-panel.offsetHeight-12))}px`;
    };
    trigger.addEventListener('click',event => {
      if (panel.matches(':popover-open')) { panel.hidePopover(); return; }
      refreshSelection(); panel.showPopover(); position();
      if (event.detail === 0) (buttons.find(button => button.dataset.themeId === selected) || document.querySelector('#custom-theme-button')).focus();
    });
    panel.addEventListener('beforetoggle',event => {
      trigger.setAttribute('aria-expanded',String(event.newState === 'open'));
      if(event.newState === 'closed')restore();
    });
    panel.addEventListener('pointerleave',restore);
    panel.addEventListener('focusout',() => queueMicrotask(() => {
      if(panel.matches(':popover-open') && !panel.contains(document.activeElement) && document.activeElement !== trigger) panel.hidePopover();
    }));
    document.querySelector('#close-theme-picker').addEventListener('click',() => { panel.hidePopover(); trigger.focus(); });
    window.addEventListener('resize',() => { if(panel.matches(':popover-open')) position(); });
    // Theme previews can change page height and trigger scroll anchoring.
    // Keep the fixed picker open; native outside-click/Escape still dismiss it.
    document.querySelector('#custom-theme-button').addEventListener('click',() => window.SlurmyCustomTheme?.open());
    window.addEventListener('slurmy:theme-saved',refreshSelection);
    refreshSelection(); restore();
  }
  function initPointerEffects() {
    let current = null, frame = null, x = 0, y = 0, smoothX = 0, smoothY = 0, lastTime = 0, angle = 0, desiredAngle = 0;
    const reduced = matchMedia('(prefers-reduced-motion: reduce)');
    const glow=document.createElement('div');glow.className='theme-cursor-glow';glow.setAttribute('aria-hidden','true');document.body.append(glow);
    const clear = () => {
      if(current)current.classList.remove('theme-hover-target');current = null;lastTime = 0;
      glow.style.setProperty('--glow-opacity','0');
      if(frame !== null){cancelAnimationFrame(frame);frame=null;}
    };
    const animate = time => {
      frame = null;
      if(!current || reduced.matches)return;
      const delta=lastTime?Math.min(40,time-lastTime):16;lastTime=time;
      const blend=hoverTuning.hoverLag===0?1:1-Math.exp(-delta/hoverTuning.hoverLag);
      smoothX+=(x-smoothX)*blend;smoothY+=(y-smoothY)*blend;
      const turn=((desiredAngle-angle+180)%360+360)%360-180;angle=(angle+turn*Math.min(1,blend*.65)+360)%360;
      glow.style.setProperty('--glow-angle',angle+'deg');
      glow.style.setProperty('--glow-x',smoothX+'px');glow.style.setProperty('--glow-y',smoothY+'px');
      if(Math.abs(x-smoothX)+Math.abs(y-smoothY)>.1 || Math.abs(turn)>.2)frame=requestAnimationFrame(animate);
    };
    document.addEventListener('pointermove',event => {
      if(reduced.matches || event.pointerType === 'touch' || !['spotlight','ribbon','haze','shimmer'].includes(root.dataset.themeHover)){clear();return;}
      if(event.target.closest?.('#theme-picker,#custom-theme-editor')){clear();return;}
      const target=event.target.closest?.('.experiment-card,.stats article,.solver-card-section,.problem-card-section,.limiter-card-section,.panel,.topbar,.sidebar') || event.target.closest?.('button,.button') || event.target.closest?.('main') || document.body;
      if(!target){clear();return;}
      if(root.dataset.themeHover==='ribbon'&&Math.hypot(event.clientX-x,event.clientY-y)>2)desiredAngle=Math.atan2(event.clientY-y,event.clientX-x)*180/Math.PI;else if(root.dataset.themeHover!=='ribbon')desiredAngle=angle;
      x=event.clientX;y=event.clientY;
      if(current !== target){
        if(current)current.classList.remove('theme-hover-target');else{smoothX=x;smoothY=y;lastTime=0;}
        current=target;current.classList.add('theme-hover-target');glow.style.setProperty('--glow-opacity','1');
      }
      if(frame === null)frame=requestAnimationFrame(animate);
    },{passive:true});
    document.addEventListener('pointerleave',clear);
    window.addEventListener('blur',clear);window.addEventListener('scroll',clear,true);
  }
  document.addEventListener('DOMContentLoaded',()=>{initPicker();initPointerEffects();});
  return {themes,fonts,normalize,cleanName,get library(){return library.map(x=>({name:x.name,settings:{...x.settings}}));},get hasCustom(){return hasCustom;},apply,select,previewCustom,saveCustom,restore:()=>apply(selected),get custom(){return {...custom};},get currentSettings(){return {...find(selected).settings};},get selected(){return selected;}};
})();
