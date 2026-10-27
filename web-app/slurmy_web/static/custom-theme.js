'use strict';

window.SlurmyCustomTheme = (() => {
  const api = window.SlurmyThemes;
  const panel = document.querySelector('#custom-theme-editor');
  const fields = document.querySelector('#custom-theme-fields');
  const base = document.querySelector('#custom-theme-base');
  const imageInput = document.querySelector('#custom-theme-image');
  const error = document.querySelector('#custom-theme-error');
  const nameInput=document.querySelector('#custom-theme-name');
  const savedSelect=document.querySelector('#custom-theme-saved');
  let draftName='My Theme';
  function refreshLibrary(){
    savedSelect.replaceChildren();
    const placeholder=document.createElement('option');placeholder.value='';placeholder.textContent='Choose a Saved Theme';savedSelect.append(placeholder);
    api.library.forEach((item,index)=>{const option=document.createElement('option');option.value=String(index);option.textContent=item.name;savedSelect.append(option);});
  }

  let editing = false, draft = null, imageRequest = 0;
  const controls = new Map();
  const schema = [
    ['Palette', [
      ['mode','Color Mode','select',[['light','Light'],['dark','Dark']]],
      ['paper','Page Background','color'],['surface','Panels & Cards','color'],['well','Inputs & Code Background','color'],
      ['ink','Main Text','color'],['muted','Secondary Text','color'],['line','Borders','color'],['accent','Accent','color'],['accentInk','Text on Accent','color'],['sidebar','Sidebar','color'],
    ]],
    ['Typography & Shape', [
      ['font','Font Style','select',[['system','Modern Sans'],['humanist','Humanist Sans'],['serif','Editorial Serif'],['rounded','Rounded'],['mono','Monospace'],['compact','Compact Sans']]],
      ['fontFamily','Installed Font Family (overrides style)','text'],['radius','Corner Radius','range',0,28,'px'],
    ]],
    ['Surfaces & Hover', [
      ['opacity','Panel Opacity','range',40,100,'%'],['blur','Glass Blur','range',0,20,'px'],
      ['hover','Hover Effect','select',[['none','None'],['lift','Gentle Lift'],['spotlight','Soft Spotlight'],['ribbon','Directional Ribbon'],['haze','Living Mist'],['shimmer','Sweeping Shimmer'],['ink','Accent Highlight']]],
      ['hoverLag','Hover Smoothing','range',0,300,'ms'],['hoverSoftness','Spotlight Softness','range',0,40,'px'],['hoverStrength','Spotlight Strength','range',1,30,'%'],
      ['transition','Transition Duration','range',0,600,'ms'],
    ]],
    ['Background & Motion', [
      ['pattern','Background Pattern','select',[['none','None'],['grid','Grid'],['dots','Dots'],['weave','Woven Paper'],['diagonal','Diagonal Lines'],['stars','Stardust']]],
      ['patternSize','Pattern Scale','range',8,100,'px'],
      ['motion','Background Animation','select',[['none','Still'],['drift','Slow Drift'],['breathe','Breathing Glow']]],
      ['imageFit','Image Fit','select',[['cover','Cover'],['contain','Contain'],['tile','Tile']]],['imageOpacity','Image Opacity','range',0,100,'%'],
    ]],
    ['Box Shadows', [
      ['shadowColor','Shadow Color','color'],['shadowY','Vertical Offset','range',0,24,'px'],['shadowBlur','Shadow Blur','range',0,60,'px'],['shadowOpacity','Shadow Strength','range',0,60,'%'],
    ]],
    ['Text Shadows', [
      ['textShadowColor','Heading Shadow Color','color'],['textShadowBlur','Heading Glow / Blur','range',0,24,'px'],['textShadowOpacity','Heading Shadow Strength','range',0,80,'%'],
    ]],
  ];
  function showError(message) { error.textContent = message; error.hidden = !message; }
  function composite(color, background=[255,255,255]) {
    const rgb=color.slice(1,7).match(/../g).map(v=>parseInt(v,16));
    const alpha=color.length===9?parseInt(color.slice(7),16)/255:1;
    return rgb.map((v,i)=>v*alpha+background[i]*(1-alpha));
  }
  function contrast(foreground,background) {
    const light = rgb => {
      const c=rgb.map(v=>v/255).map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4);
      return c[0]*.2126+c[1]*.7152+c[2]*.0722;
    };
    const paper=composite(draft.paper),surface=composite(background,paper);
    const x=light(composite(foreground,surface)),y=light(surface);
    return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);
  }
  function preview() {
    if(!editing)return;
    draft = api.normalize(draft); api.previewCustom(draft);
    const minimum = Math.min(contrast(draft.ink,draft.surface),contrast(draft.muted,draft.surface),contrast(draft.accentInk,draft.accent));
    document.querySelector('#custom-theme-contrast').textContent = minimum < 4.5
      ? `Some text has low contrast (${minimum.toFixed(1)}:1). Try darker text or a lighter surface, or vice versa.`
      : 'Text and button contrast looks good on the selected colors.';
    document.querySelector('#custom-image-name').textContent = draft.image ? `${draft.imageName || 'Background image'} · ready to preview` : 'No background image';
  }
  function fill() {
    document.querySelector('#save-custom-theme').disabled=false;
    for(const [key,control] of controls){control.input.value = control.alpha ? draft[key].slice(0,7) : draft[key];if(control.alpha){control.alpha.value=draft[key].length===9?Math.round(parseInt(draft[key].slice(7),16)/255*100):100;control.updateColor();}else if(control.output)control.output.textContent=draft[key]+control.unit;}
    preview();
  }
  for(const [title, entries] of schema) {
    const section=document.createElement('details');section.className='custom-theme-section';section.open=title==='Palette';
    const summary=document.createElement('summary');summary.textContent=title;section.append(summary);
    const grid=document.createElement('div');grid.className='custom-theme-grid';
    for(const [key,name,type,...rest] of entries){
      const label=document.createElement(type==='color'?'div':'label');label.textContent=type==='color'?'':name;
      if(type==='color')label.className='custom-color-control';
      const input=document.createElement(type==='select'?'select':'input');input.id='custom-'+key;input.dataset.themeSetting=key;label.setAttribute('for',input.id);
      if(type==='color'){const colorLabel=document.createElement('label');colorLabel.textContent=name;colorLabel.setAttribute('for',input.id);label.append(colorLabel);}
      let output=null,unit='';
      if(type==='select')for(const [value,text] of rest[0]){const option=document.createElement('option');option.value=value;option.textContent=text;input.append(option);}
      else input.type=type;
      if(type==='range'){[input.min,input.max,unit]=rest;input.step=1;output=document.createElement('output');output.htmlFor=input.id;output.setAttribute('aria-hidden','true');output.textContent='';label.append(output);}
      if(type==='text'){input.maxLength=160;input.placeholder='e.g. Verdana, sans-serif';label.className='custom-theme-wide';}
      label.append(input);grid.append(label);
      let alpha=null;
      const updateColor=()=>{
        const channels=input.value.slice(1,7).match(/../g).map(v=>parseInt(v,16));
        output.textContent=`rgba(${channels.join(', ')}, ${Number(alpha.value)/100})`;
      };
      if(type==='color'){
        alpha=document.createElement('input');alpha.type='range';alpha.min=0;alpha.max=100;alpha.step=1;
        alpha.id=input.id+'-alpha';alpha.setAttribute('aria-label',name+' Opacity');
        const alphaLabel=document.createElement('label');alphaLabel.textContent='Opacity';alphaLabel.setAttribute('for',alpha.id);
        output=document.createElement('output');output.htmlFor=alpha.id;output.className='custom-color-value';
        alphaLabel.append(alpha,output);label.append(alphaLabel);
      }
      controls.set(key,{input,output,unit,alpha,updateColor});
      const changed=()=>{
        if(!editing)return;
        if(type==='text'&&!/^[a-z0-9 ,"'_-]{0,160}$/i.test(input.value)){showError('Use installed font names separated by commas.');return;}
        showError('');draft[key]=type==='color'?input.value.slice(0,7)+(Number(alpha.value)===100?'':Math.round(Number(alpha.value)*255/100).toString(16).padStart(2,'0')):type==='range'?Number(input.value):input.value;
        if(key==='font'){draft.fontFamily='';controls.get('fontFamily').input.value='';}
        if(alpha)updateColor();else if(output)output.textContent=input.value+unit;preview();
      };
      input.addEventListener('input',changed);input.addEventListener('change',changed);
      if(alpha){alpha.addEventListener('input',changed);alpha.addEventListener('change',changed);}
    }
    section.append(grid);fields.append(section);
  }
  nameInput.addEventListener('input',()=>{draftName=nameInput.value;});
  savedSelect.addEventListener('change',()=>{
    if(savedSelect.value==='')return;
    const item=api.library[Number(savedSelect.value)];if(!item)return;
    ++imageRequest;draft={...item.settings};draftName=item.name;nameInput.value=draftName;showError('');fill();
  });
  document.querySelector('#download-custom-theme').addEventListener('click',()=>{
    const name=api.cleanName(nameInput.value)||'My Theme';
    const blob=new Blob([JSON.stringify({format:'slurmy-theme',version:1,name,settings:api.normalize(draft)},null,2)],{type:'application/json'});
    const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;
    link.download=(name.replace(/[^a-z0-9_-]+/gi,'-')||'theme')+'.slurmy-theme.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
  });
  const themeUpload=document.querySelector('#upload-custom-theme');
  themeUpload.addEventListener('change',async()=>{
    const file=themeUpload.files[0];if(!file)return;const request=++imageRequest;
    try{
      if(file.size>4*1024*1024)throw Error('Theme files must be smaller than 4 MB.');
      const data=JSON.parse(await file.text());
      if(!data||data.format!=='slurmy-theme'||data.version!==1||!data.settings||typeof data.settings!=='object'||Array.isArray(data.settings)||!api.cleanName(data.name))throw Error('Choose a valid Slurmy theme JSON file (version 1).');
      if(!editing||request!==imageRequest)return;
      draft=api.normalize(data.settings);draftName=api.cleanName(data.name);nameInput.value=draftName;showError('');fill();
    }catch(exc){if(editing&&request===imageRequest)showError(exc instanceof SyntaxError?'This file is not valid JSON.':exc.message);}
    finally{themeUpload.value='';}
  });
  for(const item of api.themes){const option=document.createElement('option');option.value=item.id;option.textContent=item.name;base.append(option);}
  base.addEventListener('change',()=>{const item=api.themes.find(item=>item.id===base.value);if(item){++imageRequest;draft={...item.settings};showError('');fill();}});
  imageInput.addEventListener('change',async()=>{
    const file=imageInput.files[0];if(!file)return;const request=++imageRequest;
    const imageError=document.querySelector('#custom-image-error');imageError.hidden=true;imageError.textContent='';
    document.querySelector('#save-custom-theme').disabled=true;
    document.querySelector('#custom-image-name').textContent=`Loading ${file.name || 'image'}…`;
    try{
      const prepared=await window.SlurmyBackgroundImage.prepare(file);
      if(!editing||request!==imageRequest)return;
      draft.image=prepared.image;draft.imageName=prepared.imageName;showError('');preview();
      if(prepared.optimized)document.querySelector('#custom-image-name').textContent=`${prepared.imageName} · optimized and ready to preview`;
    }catch(exc){
      if(editing&&request===imageRequest){
        imageError.textContent=exc.message || 'Could not open that image.';imageError.hidden=false;
        document.querySelector('#custom-image-name').textContent=draft.image?'Previous background kept':'No background image';
      }
    }finally{
      if(request===imageRequest){imageInput.value='';if(editing)document.querySelector('#save-custom-theme').disabled=false;}
    }
  });
  document.querySelector('#clear-custom-image').addEventListener('click',()=>{++imageRequest;draft.image='';draft.imageName='';document.querySelector('#custom-image-error').hidden=true;document.querySelector('#save-custom-theme').disabled=false;showError('');preview();});
  const discard=()=>panel.hidePopover();
  document.querySelector('#close-custom-theme').addEventListener('click',discard);
  document.querySelector('#discard-custom-theme').addEventListener('click',discard);
  document.querySelector('#save-custom-theme').addEventListener('click',()=>{
    try{if(!api.cleanName(nameInput.value)){showError('Give your theme a name before saving.');nameInput.focus();return;}api.saveCustom(draft,nameInput.value);draftName=api.cleanName(nameInput.value);window.dispatchEvent(new CustomEvent('slurmy:theme-saved'));panel.hidePopover();}
    catch{showError('Could not save in this browser. Try removing the background image to free space, or allow browser storage.');}
  });
  panel.addEventListener('beforetoggle',event=>{
    if(event.newState==='closed'){editing=false;++imageRequest;api.restore();document.querySelector('#theme-toggle').focus();}
  });
  document.addEventListener('keydown',event=>{if(editing&&event.key==='Escape'&&!document.querySelector('dialog[open]')){event.preventDefault();event.stopPropagation();discard();}},true);
  function open() {
    if(editing)return;editing=true;++imageRequest;
    draft=api.hasCustom?api.custom:api.currentSettings;draftName=api.library.find(item=>JSON.stringify(item.settings)===JSON.stringify(draft))?.name||draftName;nameInput.value=draftName;refreshLibrary();base.value='';showError('');imageInput.value='';document.querySelector('#custom-image-error').hidden=true;
    const picker=document.querySelector('#theme-picker');if(picker.matches(':popover-open'))picker.hidePopover();
    panel.showPopover();fill();document.querySelector('#custom-theme-title').focus();
  }
  return {open,get isOpen(){return editing;}};
})();
