'use strict';
if(document.body.dataset.page==='new'){
 const form=$('#builder');let step=0, reviewed=null;const initial=JSON.parse($('#builder-initial').textContent||'{}');const context=$('#builder-context');const editing=context?.dataset.editing==='true';
 const sections=[...form.querySelectorAll('[data-step]')];
 function prefillExamples(root){root.querySelectorAll('input[placeholder],textarea[placeholder]').forEach(input=>{if(!input.value)input.value=input.placeholder;});}
 function add(template,target){const node=$('#'+template).content.firstElementChild.cloneNode(true);prefillExamples(node);$('.remove',node).addEventListener('click',()=>node.remove());$('#'+target).append(node);window.initCollapsibles?.();return node;}
 $('#add-configuration').addEventListener('click',()=>add('configuration-template','configurations'));
 $('#add-glob').addEventListener('click',()=>add('glob-template','globs'));
 $('#add-resource').addEventListener('click',()=>add('resource-template','resources'));
 function fill(node,values){for(const [name,value] of Object.entries(values||{})){const field=node.querySelector(`[name="${name}"]`);if(field)field.value=value??'';}}
 for(const name of ['name','host','partition','degree','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter']){const field=form.querySelector(`[name="${name}"]`);if(field&&initial[name]!==undefined)field.value=initial[name];}
 const configurations=initial.configurations?.length?initial.configurations:[null];for(const values of configurations)fill(add('configuration-template','configurations'),values);
 const globs=initial.globs?.length?initial.globs:[null];for(const value of globs)fill(add('glob-template','globs'),value===null?null:{glob:value});
 const resources=initial.resources?.length?initial.resources:[null];for(const values of resources)fill(add('resource-template','resources'),values);
 prefillExamples(form);
 const picker=$('#path-browser');let pickedInput=null,pickerDirectory='',pickedPathCallback=null;
 async function browse(directory,fallback=true){
  try{const data=await api('/api/browse-path',{directory});pickerDirectory=data.directory;$('#path-location').value=data.directory;const items=$('#path-items');items.replaceChildren();
   for(const item of data.items){const row=button(`${item.directory?'📁':'📄'}  ${item.name}`,()=>{if(item.directory)browse(item.path);else if(pickedInput?.dataset.path==='file'){pickedInput.value=item.path;picker.close();if(pickedPathCallback){const callback=pickedPathCallback;pickedPathCallback=null;callback(item.path);}else validatePath(pickedInput);}},'path-item');row.disabled=!item.directory&&pickedInput?.dataset.path!=='file';items.append(row);}
   text('#path-help',data.truncated?'Showing the first 1,000 entries. Type a more specific directory above.':pickedInput?.dataset.path==='file'?'Open folders or choose a file.':'Open folders, then choose the current directory.');$('#path-parent').dataset.path=data.parent;
  }catch(error){if(fallback&&directory)browse('',false);else text('#path-help',error.message);}
 }
 form.querySelectorAll('[data-path]').forEach(input=>{const choose=button('Browse…',()=>{pickedInput=input;const current=input.value.trim();let start=current;
   if(input.dataset.path==='glob'){
    const cuts=['*','?','['].map(mark=>current.indexOf(mark)).filter(index=>index>=0);
    const cut=cuts.length?Math.min(...cuts):current.length;
    start=current.slice(0,cut).replace(/\/$/,'');
   }
   if(input.dataset.path==='file')start=current.replace(/\/[^/]*$/,'');
   $('#path-select-directory').hidden=input.dataset.path==='file';picker.showModal();browse(start.startsWith('/')?start:'');},'secondary path-browse');
   if(input.dataset.path==='glob'){const row=el('div',undefined,'path-input-row');input.replaceWith(row);row.append(input,choose);}else input.insertAdjacentElement('afterend',choose);
 });
 picker.addEventListener('close',()=>{pickedPathCallback=null;});
 $('#path-parent').addEventListener('click',()=>browse($('#path-parent').dataset.path));$('#path-go').addEventListener('click',()=>browse($('#path-location').value));$('#path-location').addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();browse(event.target.value);}});
 $('#path-select-directory').addEventListener('click',()=>{if(!pickedInput)return;const kind=pickedInput.dataset.path;if(kind==='file')return;pickedInput.value=pickerDirectory+(kind==='glob'?'/**/*.p':'');picker.close();validatePath(pickedInput);});
 $('#limiter-browse').addEventListener('click',()=>{const invocation=$('[name="limiter"]');const current=invocation.value.trim()||invocation.placeholder;const first=current.match(/^(?:'[^']*'|"[^"]*"|\S+)/)?.[0]||'';const binary=first.replace(/^['"]|['"]$/g,'');pickedInput={dataset:{path:'file'},value:binary};pickedPathCallback=path=>{const rest=current.slice(first.length).trimStart();const quoted="'"+path.replace(/'/g,"'\\''")+"'";invocation.value=quoted+(rest?' '+rest:'');};$('#path-select-directory').hidden=true;picker.showModal();browse(binary.startsWith('/')?binary.replace(/\/[^/]*$/,''):'');});
 // Repeated card fields are read from their card, never from a flattened form.
 function rootValue(name){return form.querySelector(`[name="${name}"]`).value;}
 function visibility(){
  const mode=rootValue('mode');document.querySelectorAll('[data-mode]').forEach(n=>n.hidden=n.dataset.mode!==mode);$('#problem-selection').hidden=!['interactive','configurations'].includes(mode);
  $('#building-existing').hidden=rootValue('building_mode')!=='existing';$('#building-create').hidden=rootValue('building_mode')!=='create';
  $('#limiter-existing').hidden=rootValue('limiter_mode')!=='existing';$('#limiter-inline').hidden=rootValue('limiter_mode')!=='inline';
  document.querySelectorAll('.resource').forEach(card=>{const choice=$('.resource-mode',card).value;card.querySelectorAll('[data-recipe]').forEach(n=>n.hidden=n.dataset.recipe!==choice);});
 }
 form.addEventListener('change',visibility);visibility();
 async function validatePath(input){
  const status=$('.validation',input.closest('label'));if(!input.value.trim()){if(status)status.textContent='';return false;}
  const original=input.value;if(status){status.className='validation';status.textContent='Checking…';}
  try{const result=await api('/api/validate-path',{kind:input.dataset.path,value:original});if(input.value!==original)return false;if(status){status.className='validation valid';status.textContent=result.count?`${result.count} files matched · ${result.path}`:`Found · ${result.path}`;}input.dataset.validated=original;return true;}
  catch(error){if(input.value===original&&status){status.className='validation invalid';status.textContent=error.message;}delete input.dataset.validated;return false;}
 }
 form.addEventListener('focusout',event=>{if(event.target.matches('[data-path]'))validatePath(event.target);});
 function fields(card){return Object.fromEntries([...card.querySelectorAll('[name]')].map(n=>[n.name,n.value]));}
 function payload(){const names=['name','host','partition','degree','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter'];const data=Object.fromEntries(names.map(n=>[n,rootValue(n)]));data.configurations=[...document.querySelectorAll('.configuration')].map(fields);data.resources=[...document.querySelectorAll('.resource')].map(fields);data.globs=[...document.querySelectorAll('[name="glob"]')].map(n=>n.value);return data;}
 function renderStep(){sections.forEach((s,i)=>s.hidden=i!==step);document.querySelectorAll('#steps li').forEach((n,i)=>{n.classList.toggle('current',i===step);const tab=$('button',n);if(i===step)tab.setAttribute('aria-current','step');else tab.removeAttribute('aria-current');});$('#builder-back').hidden=step===0;$('#builder-next').hidden=step===4;$('#builder-save').hidden=step!==4;text('#step-count',`Step ${step+1} of 5`);$('#builder-error').hidden=true;}
 function showError(error){text('#builder-error',error.message);$('#builder-error').hidden=false;}
 async function validSection(){
  const inputs=[...sections[step].querySelectorAll('input,textarea,select')].filter(n=>!n.closest('[hidden]'));
  for(const input of inputs){if(!input.value.trim()){input.focus();throw new Error('Fill in each visible setting before continuing.');}if(!input.checkValidity()){input.reportValidity();throw new Error('Please correct the highlighted field.');}}
  for(const input of inputs.filter(n=>n.dataset.path)){if(input.dataset.validated!==input.value&&!await validatePath(input))throw new Error('Correct the path highlighted above.');}
 }
 async function prepareReview(){reviewed=payload();const query=editing?'?'+params({directory:context.dataset.directory}):'';const data=await api('/api/preview'+query,reviewed);text('#review-summary',`${data.count} explicit calls → ${data.directory}`);const files=$('#review-files');files.replaceChildren();for(const [name,body] of Object.entries(data.files)){if(name.startsWith('.'))continue;const details=el('details');details.append(el('summary',name),el('pre',body));files.append(details);}}
 let navigating=false;
 async function navigate(target){if(navigating||target===step)return;navigating=true;const tabs=[...document.querySelectorAll('#steps button')];tabs.forEach(tab=>tab.disabled=true);$('#builder-next').disabled=true;
  try{if(target<step){step=target;renderStep();return;}for(let current=step;current<target;current++){step=current;renderStep();await validSection();if(current===3)await prepareReview();}step=target;renderStep();}
  catch(error){showError(error);}finally{navigating=false;tabs.forEach(tab=>tab.disabled=false);$('#builder-next').disabled=false;}
 }
 document.querySelectorAll('#steps button').forEach((tab,index)=>tab.addEventListener('click',()=>navigate(index)));
 $('#builder-back').addEventListener('click',()=>{step--;renderStep();});
 $('#builder-next').addEventListener('click',()=>navigate(step+1));
 $('#builder-save').addEventListener('click',async()=>{const save=$('#builder-save');save.disabled=true;try{const endpoint=editing?'/api/workflows/update?'+params({directory:context.dataset.directory}):'/api/workflows';const data=await api(endpoint,reviewed);location.href='/workflow?'+new URLSearchParams({host:reviewed.host,directory:data.directory});}catch(error){showError(error);save.disabled=false;}});
 form.addEventListener('submit',e=>e.preventDefault());renderStep();
}
