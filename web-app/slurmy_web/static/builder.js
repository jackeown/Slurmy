'use strict';
if(document.body.dataset.page==='new'){
 const form=$('#builder');let step=0, reviewed=null;const initial=JSON.parse($('#builder-initial').textContent||'{}');const context=$('#builder-context');const editing=context?.dataset.editing==='true';
 const repoRoot=document.body.dataset.repoRoot;
 const sections=[...form.querySelectorAll('[data-step]')];
 function prefillExamples(root){root.querySelectorAll('input[placeholder],textarea[placeholder]').forEach(input=>{if(!input.value)input.value=input.placeholder;});}
 function add(template,target){const node=$('#'+template).content.firstElementChild.cloneNode(true);prefillExamples(node);$('.remove',node).addEventListener('click',()=>node.remove());$('#'+target).append(node);setupBrowsers(node);window.initCollapsibles?.();return node;}
 $('#add-configuration').addEventListener('click',()=>add('configuration-template','configurations'));
 $('#add-glob').addEventListener('click',()=>add('glob-template','globs'));
 $('#add-axiom-glob').addEventListener('click',()=>add('axiom-glob-template','axiom-globs'));
 let resourceOrder=0;
 function addResource(role,values){const node=add('resource-template',role==='limiter'?'limiter-resources':'solver-resources');node.dataset.resourceRole=role;node.dataset.resourceOrder=resourceOrder++;const caption=$('h3',node);const toggle=$('.collapse-toggle',caption);caption.textContent=role==='limiter'?'Limiter root and build':'Solver root and build';if(toggle){toggle.setAttribute('aria-label',`Collapse ${caption.textContent}`);caption.prepend(toggle);}fill(node,values);return node;}
 $('#add-solver-resource').addEventListener('click',()=>{addResource('solver');visibility();});
 $('#add-limiter-resource').addEventListener('click',()=>{addResource('limiter');visibility();});
 function fill(node,values){for(const [name,value] of Object.entries(values||{})){const field=node.querySelector(`[name="${name}"]`);if(field)field.value=value??'';}}
 for(const name of ['name','host','partition','batch_size','pair_order','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter']){const field=form.querySelector(`[name="${name}"]`);if(field&&initial[name]!==undefined)field.value=initial[name];}
 if(!editing&&!initial.limiter){$('[name="limiter_mode"]').value='inline';$('[name="limiter"]').value=`${repoRoot}/build/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}`;}
 const configurations=initial.configurations?.length?initial.configurations:[null];for(const values of configurations){const card=add('configuration-template','configurations');fill(card,values);const resource=(initial.resources||[]).find(item=>item.role==='solver'&&item.root===values?.solver_directory);if(resource)fill(card,{build_mode:resource.mode,build_source:resource.source,build_script:resource.script});}
 const globs=initial.globs?.length?initial.globs:[null];for(const value of globs)fill(add('glob-template','globs'),value===null?null:{glob:value});
 for(const value of initial.axiom_globs||[])fill(add('axiom-glob-template','axiom-globs'),{axiom_glob:value});
 for(const values of initial.resources||[])if(values.role==='limiter'||initial.mode!=='interactive')addResource(values.role==='limiter'?'limiter': 'solver',values);
 if(!initial.resources?.length)addResource('limiter',editing?null:{root:`${repoRoot}/build/runsolver`,mode:'script',script:`${repoRoot}/build/runsolver/build.sh`,artifact:'runsolver',source:''});
 $('[name="axiom_mode"]').value=initial.axiom_globs?.length?'custom':'cluster';
 if(initial.building_mode==='existing')$('#build-import').open=true;
 prefillExamples(form);
 const picker=$('#path-browser');let pickedInput=null,pickerDirectory='',pickedPathCallback=null;
 async function browse(directory,fallback=true){
  try{const data=await api('/api/browse-path',{directory});pickerDirectory=data.directory;$('#path-location').value=data.directory;const items=$('#path-items');items.replaceChildren();
   for(const item of data.items){const row=button(`${item.directory?'📁':'📄'}  ${item.name}`,()=>{if(item.directory)browse(item.path);else if(pickedInput?.dataset.path==='file'){pickedInput.value=item.path;picker.close();if(pickedPathCallback){const callback=pickedPathCallback;pickedPathCallback=null;callback(item.path);}else validatePath(pickedInput);}},'path-item');row.disabled=!item.directory&&pickedInput?.dataset.path!=='file';items.append(row);}
   text('#path-help',data.truncated?'Showing the first 1,000 entries. Type a more specific directory above.':pickedInput?.dataset.path==='file'?'Open folders or choose a file.':'Open folders, then choose the current directory.');$('#path-parent').dataset.path=data.parent;
  }catch(error){if(fallback&&directory)browse('',false);else text('#path-help',error.message);}
 }
 function setupBrowsers(root){root.querySelectorAll('[data-path]').forEach(input=>{if(input.dataset.browserReady)return;input.dataset.browserReady='true';const choose=button('Browse…',()=>{pickedInput=input;const current=input.value.trim();let start=current;
   if(input.dataset.path==='glob'){
    const cuts=['*','?','['].map(mark=>current.indexOf(mark)).filter(index=>index>=0);
    const cut=cuts.length?Math.min(...cuts):current.length;
    start=current.slice(0,cut).replace(/\/$/,'');
   }
   if(input.dataset.path==='file')start=current.replace(/\/[^/]*$/,'');
   $('#path-select-directory').hidden=input.dataset.path==='file';picker.showModal();browse(start.startsWith('/')?start:'');},'secondary path-browse');
   if(input.dataset.path==='glob'){const row=el('div',undefined,'path-input-row');input.replaceWith(row);row.append(input,choose);}else input.insertAdjacentElement('afterend',choose);
 });}
 setupBrowsers(form);
 picker.addEventListener('close',()=>{pickedPathCallback=null;});
 $('#path-parent').addEventListener('click',()=>browse($('#path-parent').dataset.path));$('#path-go').addEventListener('click',()=>browse($('#path-location').value));$('#path-location').addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();browse(event.target.value);}});
 $('#path-select-directory').addEventListener('click',()=>{if(!pickedInput)return;const kind=pickedInput.dataset.path;if(kind==='file')return;pickedInput.value=pickerDirectory+(kind==='glob'?(pickedInput.name==='axiom_glob'?'/**/*.ax':'/**/*.p'):'');picker.close();validatePath(pickedInput);});
 $('#limiter-browse').addEventListener('click',()=>{const invocation=$('[name="limiter"]');const current=invocation.value.trim()||invocation.placeholder;const first=current.match(/^(?:'[^']*'|"[^"]*"|\S+)/)?.[0]||'';const binary=first.replace(/^['"]|['"]$/g,'');pickedInput={dataset:{path:'file'},value:binary};pickedPathCallback=path=>{const rest=current.slice(first.length).trimStart();const quoted="'"+path.replace(/'/g,"'\\''")+"'";invocation.value=quoted+(rest?' '+rest:'');};$('#path-select-directory').hidden=true;picker.showModal();browse(binary.startsWith('/')?binary.replace(/\/[^/]*$/,''):'');});
 // Repeated card fields are read from their card, never from a flattened form.
 function rootValue(name){return form.querySelector(`[name="${name}"]`).value;}
 function visibility(){
  const mode=rootValue('mode');document.querySelectorAll('[data-mode]').forEach(n=>n.hidden=n.dataset.mode!==mode);$('#problem-selection').hidden=!['interactive','configurations'].includes(mode);
  $('#problems-from-jobpairs').hidden=mode!=='jobpairs';$('#custom-axioms').hidden=rootValue('axiom_mode')!=='custom';
  $('#building-existing').hidden=rootValue('building_mode')!=='existing';$('#building-create').hidden=rootValue('building_mode')!=='create';
  $('#limiter-building-create').hidden=rootValue('building_mode')!=='create';$('#limiter-building-imported').hidden=rootValue('building_mode')!=='existing';$('#building-create').hidden=rootValue('building_mode')!=='create'||mode==='interactive';$('#imported-build-help').hidden=mode==='interactive';
  $('#limiter-existing').hidden=rootValue('limiter_mode')!=='existing';$('#limiter-inline').hidden=rootValue('limiter_mode')!=='inline';
  document.querySelectorAll('.resource').forEach(card=>{const choice=$('.resource-mode',card).value;card.querySelectorAll('[data-recipe]').forEach(n=>n.hidden=n.dataset.recipe!==choice);$('[data-resource-build]',card).hidden=choice==='none';});
  document.querySelectorAll('.configuration').forEach(card=>{const choice=$('.solver-build-mode',card).value;$('.solver-build',card).hidden=rootValue('building_mode')!=='create';$('[data-solver-build]',card).hidden=choice==='none';});
 }
 form.addEventListener('change',visibility);visibility();
 async function validatePath(input){
  const status=$('.validation',input.closest('label'));if(!input.value.trim()){if(status)status.textContent='';return false;}
  const original=input.value;if(status){status.className='validation';status.textContent='Checking…';}
  try{const result=await api('/api/validate-path',{kind:input.dataset.path,role:input.name==='axiom_glob'?'axiom':'problem',value:original});if(input.value!==original)return false;if(status){status.className='validation valid';status.textContent=result.count?`${result.count} files matched · ${result.path}`:`Found · ${result.path}`;}input.dataset.validated=original;return true;}
  catch(error){if(input.value===original&&status){status.className='validation invalid';status.textContent=error.message;}delete input.dataset.validated;return false;}
 }
 form.addEventListener('focusout',event=>{if(event.target.matches('[data-path]'))validatePath(event.target);});
 function fields(card){return Object.fromEntries([...card.querySelectorAll('[name]')].map(n=>[n.name,n.value]));}
 function payload(){const names=['name','host','partition','batch_size','pair_order','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter','axiom_mode'];const data=Object.fromEntries(names.map(n=>[n,rootValue(n)]));const cards=[...document.querySelectorAll('.configuration')];data.configurations=cards.map(card=>{const row=fields(card);for(const key of Object.keys(row))if(key.startsWith('build_'))delete row[key];row.cpus='auto';return row;});data.resources=[...document.querySelectorAll('.resource')].sort((a,b)=>Number(a.dataset.resourceOrder)-Number(b.dataset.resourceOrder)).filter(node=>data.mode!=='interactive'||node.dataset.resourceRole==='limiter').map(node=>({...fields(node),role:node.dataset.resourceRole}));if(data.mode==='interactive'&&data.building_mode==='create'){const seen=new Map();for(const card of cards){const value=fields(card);const resource={role:'solver',root:value.solver_directory,mode:value.build_mode,source:value.build_source,script:value.build_script,artifact:value.build_mode==='script'?solverArtifact(value.command,value.solver_directory):''};const previous=seen.get(resource.root);if(previous){if(resource.mode!=='none'&&previous.mode!=='none'&&JSON.stringify(resource)!==JSON.stringify(previous))throw new Error(`Conflicting build settings for solver root ${resource.root}`);if(previous.mode==='none'&&resource.mode!=='none')Object.assign(previous,resource);}else{seen.set(resource.root,resource);data.resources.push(resource);}}}data.globs=[...document.querySelectorAll('[name="glob"]')].map(n=>n.value);data.axiom_globs=data.axiom_mode==='custom'?[...document.querySelectorAll('[name="axiom_glob"]')].map(n=>n.value):[];return data;}
 function solverArtifact(command,root){const executable=command.trim().match(/^(?:'([^']+)'|"([^"]+)"|(\S+))/)?.slice(1).find(Boolean)||'';if(!executable)throw new Error('Enter the solver command before configuring its build.');const path=executable.startsWith(root+'/')?executable.slice(root.length+1):executable.replace(/^\.\//,'');if(path.startsWith('/')||path==='..'||path.startsWith('../')||path.includes('/../')||path.includes('{{'))throw new Error('For a remote build, start the solver command with an executable relative to the solver root.');return path;}
 function renderStep(){sections.forEach((s,i)=>s.hidden=i!==step);document.querySelectorAll('#steps li').forEach((n,i)=>{n.classList.toggle('current',i===step);const tab=$('button',n);if(i===step)tab.setAttribute('aria-current','step');else tab.removeAttribute('aria-current');});$('#builder-back').hidden=step===0;$('#builder-next').hidden=step===4;$('#builder-save').hidden=step!==4;text('#step-count',`Step ${step+1} of 5`);$('#builder-error').hidden=true;}
 function showError(error){text('#builder-error',error.message);$('#builder-error').hidden=false;}
 async function validSection(){
  const inputs=[...sections[step].querySelectorAll('input,textarea,select')].filter(n=>!n.closest('[hidden]'));
  for(const input of inputs){if(!input.value.trim()&&!input.dataset.optional){input.focus();throw new Error('Fill in each visible setting before continuing.');}if(!input.checkValidity()){input.reportValidity();throw new Error('Please correct the highlighted field.');}}
  for(const input of inputs.filter(n=>n.dataset.path&&n.value.trim())){if(input.dataset.validated!==input.value&&!await validatePath(input))throw new Error('Correct the path highlighted above.');}
 }
 const fileDescriptions={'jobpairs.csv':'One row per solver call and problem, with its limits and placement requirements.','configurations.csv':'The reusable solver configurations from which the call list is generated.','problem-globs.txt':'The problem paths, globs, or archives used to generate calls.','axiom-globs.txt':'Custom TPTP axiom files to package for include statements.','building.txt':'Runtime roots, source roots, build scripts, and expected executables.','resource_limiter_template.txt':'The command template that wraps each solver call.','Makefile':'Prepare, build, submit, sync, and monitor commands for this workflow.'};
 async function prepareReview(){if(!rootValue('batch_size'))form.querySelector('[name="batch_size"]').value='1';reviewed=payload();const query=editing?'?'+params({directory:context.dataset.directory}):'';const data=await api('/api/preview'+query,reviewed);text('#review-summary',`${data.count} explicit calls → ${data.directory}`);const files=$('#review-files');files.replaceChildren();for(const [name,body] of Object.entries(data.files)){if(name.startsWith('.'))continue;const details=el('details');details.append(el('summary',name),el('p',fileDescriptions[name]||'Remote build recipe used when submitting this workflow.'),el('pre',body));files.append(details);}}
 for(const name of ['pair_order','batch_size'])form.querySelector(`[name="${name}"]`).addEventListener('change',()=>{if(step===4)prepareReview().catch(showError);});
 let navigating=false;
 async function navigate(target){if(navigating||target===step)return;navigating=true;const tabs=[...document.querySelectorAll('#steps button')];tabs.forEach(tab=>tab.disabled=true);$('#builder-next').disabled=true;
  try{if(target<step){step=target;renderStep();return;}for(let current=step;current<target;current++){step=current;renderStep();await validSection();if(current===3)await prepareReview();}step=target;renderStep();}
  catch(error){showError(error);}finally{navigating=false;tabs.forEach(tab=>tab.disabled=false);$('#builder-next').disabled=false;}
 }
 document.querySelectorAll('#steps button').forEach((tab,index)=>tab.addEventListener('click',()=>navigate(index)));
 $('#builder-back').addEventListener('click',()=>{step--;renderStep();});
 $('#builder-next').addEventListener('click',()=>navigate(step+1));
 $('#builder-save').addEventListener('click',async()=>{const save=$('#builder-save');save.disabled=true;try{await validSection();await prepareReview();const endpoint=editing?'/api/workflows/update?'+params({directory:context.dataset.directory}):'/api/workflows';const data=await api(endpoint,reviewed);location.href='/workflow?'+new URLSearchParams({host:reviewed.host,directory:data.directory});}catch(error){showError(error);save.disabled=false;}});
 form.addEventListener('submit',e=>e.preventDefault());renderStep();
}
