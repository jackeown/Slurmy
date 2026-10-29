'use strict';
// Font Awesome Free glyphs are decorative; action text and accessible names stay
// intact when controls are created, relabelled, or moved by other UI code.
window.SlurmyActionIcons=(()=>{
  const selector='button,a.button,label.button,a.back';
  const rules=[
    [/3d|comparison/i,'cube'],[/recovery|archive/i,'box-archive'],
    [/duplicat|copy|import.*configuration/i,'copy'],[/download/i,'download'],[/upload|import/i,'upload'],
    [/restor/i,'clock-rotate-left'],[/delet|remove permanently/i,'trash-can'],[/remov|clear/i,'xmark'],
    [/renam|edit/i,'pen'],[/refresh|try again|retry/i,'rotate'],[/stop|kill/i,'stop'],[/resum|start/i,'play'],
    [/prepar|build/i,'gears'],[/dispatch|submit/i,'paper-plane'],[/sav|create copy/i,'floppy-disk'],
    [/browse|directory|folder|path/i,'folder-open'],[/connect/i,'plug'],[/new|add/i,'plus'],
    [/close|cancel|discard|hide/i,'xmark'],[/inspect|view|show|explore/i,'eye'],
    [/previous|back/i,'arrow-left'],[/next|continue|go\b/i,'arrow-right'],[/parent/i,'arrow-up'],
    [/theme|palette/i,'palette'],[/choose|select|use|done|got it|confirm|apply/i,'check'],
    [/loading|packaging|validating|working/i,'hourglass-half'],[/more .*options|more .*actions|parent directories/i,'chevron-down'],
  ];
  function iconFor(label){
    const clean=label.replace(/^[＋+↓↑↻←→▣✧✕×]\s*/,'');
    if(/^more .*options|^more .*actions|^show parent directories/i.test(clean))return 'chevron-down';
    const primary=clean.match(/^(duplicate|copy|rename|edit|delete|remove|restore|download|upload|refresh|stop|resume|save|cancel|close|discard|connect|browse|new|add)\b/i)?.[0].toLowerCase();
    const actions={duplicate:'copy',copy:'copy',rename:'pen',edit:'pen',delete:'trash-can',remove:'trash-can',restore:'clock-rotate-left',download:'download',upload:'upload',refresh:'rotate',stop:'stop',resume:'play',save:'floppy-disk',cancel:'xmark',close:'xmark',discard:'xmark',connect:'plug',browse:'folder-open',new:'plus',add:'plus'};
    return actions[primary]||rules.find(([pattern])=>pattern.test(clean))?.[1];
  }
  function decorate(control){
    if(control.matches('.collapse-toggle,.sort-button,.theme-option,.outcome-row-button,.cluster-node-button,.theme-toggle')||control.closest('template'))return;
    const label=(control.getAttribute('aria-label')||control.textContent||'').trim();
    let icon=iconFor(label);
    if(!icon && control.textContent.trim()==='✕')icon='xmark';
    if(!icon){if(control.dataset.actionIcon)delete control.dataset.actionIcon;return;}
    if(control.dataset.actionIcon!==icon)control.dataset.actionIcon=icon;
    // Replace the old Unicode stand-ins rather than drawing two icons.
    for(const child of control.childNodes){
      if(child.nodeType!==3)continue;
      const clean=child.nodeValue.replace(/^\s*[＋+↓↑↻←→▣✧✕×]\s*/,'').replace(/\s*[→←]$/,'');
      if(clean!==child.nodeValue){if(!clean.trim()&&!control.getAttribute('aria-label'))control.setAttribute('aria-label',label);child.nodeValue=clean;}
    }
    if(control.id==='rename-job')control.querySelector('svg')?.remove();
    if(/^[▾▴]$/.test(control.textContent.trim())){control.textContent='';control.dataset.actionIcon='chevron-down';}
    control.classList.toggle('action-icon-only',!control.textContent.trim()&&!control.querySelector('input,small'));
  }
  function scan(root){if(root.nodeType===1&&root.matches(selector))decorate(root);root.querySelectorAll?.(selector).forEach(decorate);}
  let pending=new Set(),frame=null;
  function schedule(node){
    const element=node.nodeType===1?node:node.parentElement;if(!element)return;
    pending.add(element.closest(selector)||element);
    if(frame===null)frame=requestAnimationFrame(()=>{frame=null;const roots=pending;pending=new Set();roots.forEach(root=>{if(root.isConnected)scan(root);});});
  }
  document.addEventListener('DOMContentLoaded',()=>{
    scan(document);
    new MutationObserver(records=>{for(const record of records){if(record.type==='childList'){schedule(record.target);record.addedNodes.forEach(schedule);}else schedule(record.target);}})
      .observe(document.body,{childList:true,subtree:true,characterData:true,attributes:true,attributeFilter:['aria-label']});
  });
  return {iconFor,scan};
})();
