'use strict';
if(document.body.dataset.page==='new'){
 const form=$('#builder');let step=0, reviewed=null;
 const sections=[...form.querySelectorAll('[data-step]')];
 function add(template,target){const node=$('#'+template).content.firstElementChild.cloneNode(true);$('.remove',node).addEventListener('click',()=>node.remove());$('#'+target).append(node);return node;}
 $('#add-configuration').addEventListener('click',()=>add('configuration-template','configurations'));
 $('#add-glob').addEventListener('click',()=>add('glob-template','globs'));
 $('#add-resource').addEventListener('click',()=>add('resource-template','resources'));
 add('configuration-template','configurations');add('glob-template','globs');add('resource-template','resources');
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
 function renderStep(){sections.forEach((s,i)=>s.hidden=i!==step);document.querySelectorAll('#steps li').forEach((n,i)=>n.classList.toggle('current',i===step));$('#builder-back').hidden=step===0;$('#builder-next').hidden=step===4;$('#builder-save').hidden=step!==4;text('#step-count',`Step ${step+1} of 5`);$('#builder-error').hidden=true;}
 function showError(error){text('#builder-error',error.message);$('#builder-error').hidden=false;}
 async function validSection(){
  const inputs=[...sections[step].querySelectorAll('input,textarea,select')].filter(n=>!n.closest('[hidden]'));
  for(const input of inputs){if(!input.value.trim()){input.focus();throw new Error('Fill in each visible setting before continuing.');}if(!input.checkValidity()){input.reportValidity();throw new Error('Please correct the highlighted field.');}}
  for(const input of inputs.filter(n=>n.dataset.path)){if(input.dataset.validated!==input.value&&!await validatePath(input))throw new Error('Correct the path highlighted above.');}
 }
 $('#builder-back').addEventListener('click',()=>{step--;renderStep();});
 $('#builder-next').addEventListener('click',async()=>{
  const next=$('#builder-next');next.disabled=true;
  try{await validSection();if(step===3){reviewed=payload();const data=await api('/api/preview',reviewed);text('#review-summary',`${data.count} explicit calls → ${data.directory}`);const files=$('#review-files');files.replaceChildren();for(const [name,body] of Object.entries(data.files)){const details=el('details');details.append(el('summary',name),el('pre',body));files.append(details);}}step++;renderStep();}
  catch(error){showError(error);}finally{next.disabled=false;}
 });
 $('#builder-save').addEventListener('click',async()=>{const save=$('#builder-save');save.disabled=true;try{const data=await api('/api/experiments',reviewed);location.href='/experiment?'+new URLSearchParams({host:reviewed.host,directory:data.directory});}catch(error){showError(error);save.disabled=false;}});
 form.addEventListener('submit',e=>e.preventDefault());renderStep();
}
