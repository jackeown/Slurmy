'use strict';
window.SlurmySections=(()=>{
  const running=new WeakMap();
  const isDetails=section=>section.tagName==='DETAILS';
  const isCollapsed=section=>running.get(section)?.collapsed??(isDetails(section)?!section.open:section.classList.contains('collapsed'));
  function toggle(section,collapsed){
    const previous=running.get(section),from=section.getBoundingClientRect().height;
    const painted=previous?new Map([...section.children].map(child=>[child,{opacity:getComputedStyle(child).opacity,transform:getComputedStyle(child).transform}])):null;
    if(previous){previous.animations.forEach(animation=>animation.cancel());previous.restore();running.delete(section);}
    const native=isDetails(section),header=native?section.querySelector(':scope > summary'):section.querySelector(':scope > .collapse-header');
    if(!header)return;
    const apply=closed=>{if(native)section.open=!closed;else section.classList.toggle('collapsed',closed);};
    const css=getComputedStyle(document.documentElement);
    const duration=matchMedia('(prefers-reduced-motion: reduce)').matches?0:parseFloat(css.getPropertyValue('--section-duration'))||0;
    if(!duration||typeof section.animate!=='function'){apply(collapsed);return;}
    apply(collapsed);
    const to=section.getBoundingClientRect().height;
    apply(false);
    const saved={height:section.style.height,overflow:section.style.overflow,boxSizing:section.style.boxSizing};
    const restore=()=>{Object.assign(section.style,saved);delete section.dataset.sectionState;};
    section.style.boxSizing='border-box';section.style.overflow='clip';section.dataset.sectionState=collapsed?'closing':'opening';
    const easing=css.getPropertyValue('--section-easing').trim()||'ease';
    const style=css.getPropertyValue('--section-style').trim()||'slide-fade';
    const animations=[section.animate([{height:from+'px'},{height:to+'px'}],{duration,easing})];
    if(style!=='slide')for(const child of section.children){
      if(child===header)continue;
      const frames=collapsed?[{opacity:1},{opacity:0}]:[{opacity:0},{opacity:1}];
      if(style==='slide-fade')frames.forEach((frame,index)=>frame.transform=`translateY(${(collapsed?index:1-index)*4}px)`);
      if(painted?.has(child))Object.assign(frames[0],painted.get(child));
      animations.push(child.animate(frames,{duration:collapsed?duration*.8:duration,easing}));
    }
    const state={collapsed,animations,restore};running.set(section,state);
    animations[0].finished.then(()=>{
      if(running.get(section)!==state)return;
      apply(collapsed);animations.forEach(animation=>animation.cancel());restore();running.delete(section);
    }).catch(()=>{});
  }
  document.addEventListener('click',event=>{
    const summary=event.target.closest('summary');
    if(!summary||event.target.closest('button,a,input,select,textarea,label'))return;
    const section=summary.parentElement;if(!isDetails(section)||summary!==section.querySelector(':scope > summary'))return;
    event.preventDefault();toggle(section,!isCollapsed(section));
  });
  return {toggle,isCollapsed};
})();
