'use strict';
if(document.body.dataset.page==='new'){
 const form=$('#builder');let step=0, reviewed=null;const initial=JSON.parse($('#builder-initial').textContent||'{}');const context=$('#builder-context');const editing=context?.dataset.editing==='true';
 const repoRoot=document.body.dataset.repoRoot;
 const sections=[...form.querySelectorAll('[data-step]')];
 function prefillExamples(root){root.querySelectorAll('input[placeholder],textarea[placeholder]').forEach(input=>{if(!input.value)input.value=input.placeholder;});}
 function add(template,target){const node=$('#'+template).content.firstElementChild.cloneNode(true);prefillExamples(node);if(node.matches('.configuration'))node.querySelector('[name="solver_name"]').value='';$('.remove',node).addEventListener('click',()=>{node.remove();if(target==='globs'||target==='axiom-globs')updateProblemStatus();if(target==='limiter-resources')updateLimiterStatus();updateStepStates();});$('#'+target).append(node);setupBrowsers(node);shellEditors(node);window.initCollapsibles?.();if(node.matches('.configuration'))updateSolverStatus(node);if(target==='globs'||target==='axiom-globs')updateProblemStatus();if(target==='limiter-resources')updateLimiterStatus();updateStepStates();return node;}
 $('#add-configuration').addEventListener('click',()=>{const card=add('configuration-template','configurations');$('#builder-error').hidden=true;visibility();card.scrollIntoView({behavior:'smooth',block:'start'});});
 $('#add-glob').addEventListener('click',()=>add('glob-template','globs'));
 $('#add-axiom-glob').addEventListener('click',()=>add('axiom-glob-template','axiom-globs'));
 let resourceOrder=0;
 function addResource(role,values){const node=add('resource-template',role==='limiter'?'limiter-resources':'solver-resources');node.dataset.resourceRole=role;node.dataset.resourceOrder=resourceOrder++;const caption=$('h3',node);const toggle=$('.collapse-toggle',caption);caption.textContent=role==='limiter'?'Limiter resource':'Cluster build for imported solver';if(toggle){toggle.setAttribute('aria-label',`Collapse ${caption.textContent}`);caption.prepend(toggle);}if(role==='solver')node.querySelector('[name="artifact"]').placeholder='solver';else $('.remove',node).hidden=true;fill(node,values);if(role==='limiter')updateLimiterStatus();return node;}
 $('#add-solver-resource').addEventListener('click',()=>{addResource('solver',{mode:'script',artifact:''});visibility();});
 function configurationTitle(card){const name=card.querySelector('[name="solver_name"]').value.trim(),caption=$('h3',card);const toggle=$('.collapse-toggle',caption);caption.textContent=name?`${name} Configuration`:'Solver configuration';if(toggle){toggle.setAttribute('aria-label',`${card.classList.contains('collapsed')?'Expand':'Collapse'} ${caption.textContent}`);caption.prepend(toggle);}}
 function fill(node,values){for(const [name,value] of Object.entries(values||{})){const field=node.querySelector(`[name="${name}"]`);if(field){field.value=value??'';field.refreshShell?.();}}if(node.matches('.configuration')){configurationTitle(node);updateSolverStatus(node);}}
 for(const name of ['name','host','partition','batch_size','pair_order','mode','jobpairs','configurations_file','limiter_mode','limiter_file','limiter']){const field=form.querySelector(`[name="${name}"]`);if(field&&initial[name]!==undefined)field.value=initial[name];}
 $('[name="limiter_mode"]').value='inline';$('[name="limiter_file"]').value='';
 if(!editing&&!initial.limiter){$('[name="limiter_mode"]').value='inline';$('[name="limiter"]').value=`${repoRoot}/implementation/build/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --timestamp --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}`;}
 for(const values of initial.configurations||[]){const card=add('configuration-template','configurations');fill(card,{...values,solver_name:values.solver_name||values.command?.trim().split(/\s+/)[0]?.split('/').pop()||''});const resource=(initial.resources||[]).find(item=>item.role==='solver'&&item.root===values.solver_directory);if(resource)fill(card,{build_mode:resource.mode,build_source:resource.source,build_script:resource.script});}
 const globs=initial.globs?.length?initial.globs:[null];for(const value of globs)fill(add('glob-template','globs'),value===null?null:{glob:value});
 for(const value of initial.axiom_globs||[])fill(add('axiom-glob-template','axiom-globs'),{axiom_glob:value});
 let limiterLoaded=false;
 for(const values of initial.resources||[])if(values.role==='limiter'){if(!limiterLoaded){addResource('limiter',values);limiterLoaded=true;}}else if(initial.mode!=='interactive')addResource('solver',values);
 const copyDialog=$('#copy-configuration-dialog'),copyWorkflow=$('#copy-workflow'),copySolver=$('#copy-solver'),copyPreview=$('#copy-configuration-preview'),copyConfirm=$('#confirm-copy-configuration');
 let copyChoices=[],copyRequest=0;
 function copyError(message){text('#copy-configuration-error',message);$('#copy-configuration-error').hidden=!message;}
 $('#close-copy-configuration').addEventListener('click',()=>copyDialog.close());
 $('#copy-configuration').addEventListener('click',async()=>{
  const request=++copyRequest;copyChoices=[];copyDialog.showModal();copyError('');copyWorkflow.replaceChildren(el('option','Loading workflows…'));copyWorkflow.disabled=true;copySolver.replaceChildren(el('option','Choose a workflow first…'));copySolver.disabled=true;copyConfirm.disabled=true;copyPreview.textContent='Loading workflows…';
  try{const data=await api('/api/solver-configurations?'+params({}));if(request!==copyRequest)return;copyWorkflow.replaceChildren(el('option','Choose a workflow…'));copyWorkflow.firstChild.value='';for(const item of data.workflows){const option=el('option',`${item.group} · ${item.name}`);option.value=item.directory;copyWorkflow.append(option);}copyWorkflow.disabled=false;copyPreview.textContent=data.workflows.length?'Select a workflow to see its configurations.':'No workflows are available yet.';}
  catch(error){if(request!==copyRequest)return;copyError(error.message);copyPreview.textContent='Could not load workflows.';}
 });
 copyWorkflow.addEventListener('change',async()=>{
  const request=++copyRequest;copyChoices=[];copySolver.replaceChildren(el('option','Loading configurations…'));copySolver.disabled=true;copyConfirm.disabled=true;copyPreview.textContent='Loading configurations…';copyError('');
  if(!copyWorkflow.value){copySolver.firstChild.textContent='Choose a workflow first…';copyPreview.textContent='Select a workflow to see its configurations.';return;}
  try{const data=await api('/api/solver-configurations?'+params({directory:copyWorkflow.value}));if(request!==copyRequest)return;copyChoices=data.configurations||[];copySolver.replaceChildren(el('option',copyChoices.length?'Choose a solver configuration…':'This workflow has no solver configurations.'));copySolver.firstChild.value='';copyChoices.forEach((item,index)=>{const option=el('option',`${index+1} · ${item.solver_name} — ${item.solver_directory}`);option.value=String(index);copySolver.append(option);});copySolver.disabled=!copyChoices.length;copyPreview.textContent=copyChoices.length?`${copyChoices.length} configuration${copyChoices.length===1?'':'s'} available.`:'There is nothing to copy from this workflow.';}
  catch(error){if(request!==copyRequest)return;copyError(error.message);copyPreview.textContent='Could not load configurations.';}
 });
 copySolver.addEventListener('change',()=>{const choice=copySolver.value?copyChoices[Number(copySolver.value)]:null;copyConfirm.disabled=!choice;if(choice)highlightShell(copyPreview,`${choice.command}\nRoot: ${choice.solver_directory}\nLimits: ${choice.cpu_limit}s CPU, ${choice.wc_limit}s wall, ${choice.mem_limit}, ${choice.cores} core(s)\nBuild: ${choice.build_mode==='script'?choice.build_script:'none'}`);else copyPreview.textContent='Choose a solver configuration to preview it.';});
 copyConfirm.addEventListener('click',()=>{const choice=copyChoices[Number(copySolver.value)];if(!copySolver.value||!choice)return;const card=add('configuration-template','configurations');fill(card,choice);$('#builder-error').hidden=true;visibility();copyDialog.close();card.scrollIntoView({behavior:'smooth',block:'center'});for(const input of card.querySelectorAll('[data-path]'))if(input.value.trim())validatePath(input);});
 const limiterDialog=$('#copy-limiter-dialog'),limiterWorkflow=$('#copy-limiter-workflow'),limiterPreview=$('#copy-limiter-preview'),limiterConfirm=$('#confirm-copy-limiter');
 let limiterChoice=null,limiterRequest=0;
 function limiterCopyError(message){text('#copy-limiter-error',message);$('#copy-limiter-error').hidden=!message;}
 $('#close-copy-limiter').addEventListener('click',()=>limiterDialog.close());
 $('#copy-limiter').addEventListener('click',async()=>{
  const request=++limiterRequest;limiterChoice=null;limiterDialog.showModal();limiterCopyError('');limiterWorkflow.replaceChildren(el('option','Loading workflows…'));limiterWorkflow.disabled=true;limiterConfirm.disabled=true;limiterPreview.textContent='Loading workflows…';
  try{const data=await api('/api/solver-configurations?'+params({}));if(request!==limiterRequest)return;limiterWorkflow.replaceChildren(el('option','Choose a workflow…'));limiterWorkflow.firstChild.value='';for(const item of data.workflows){const option=el('option',`${item.group} · ${item.name}`);option.value=item.directory;limiterWorkflow.append(option);}limiterWorkflow.disabled=false;limiterPreview.textContent=data.workflows.length?'Select a workflow to preview its limiter.':'No workflows are available yet.';}
  catch(error){if(request!==limiterRequest)return;limiterCopyError(error.message);limiterPreview.textContent='Could not load workflows.';}
 });
 limiterWorkflow.addEventListener('change',async()=>{
  const request=++limiterRequest;limiterChoice=null;limiterConfirm.disabled=true;limiterCopyError('');limiterPreview.textContent=limiterWorkflow.value?'Loading limiter…':'Select a workflow to preview its limiter.';if(!limiterWorkflow.value)return;
  try{const choice=await api('/api/limiter-configuration?'+params({directory:limiterWorkflow.value}));if(request!==limiterRequest)return;limiterChoice=choice;highlightShell(limiterPreview,`${choice.invocation}\nRoot: ${choice.resource.root}\nBuild: ${choice.resource.mode==='script'?choice.resource.script:choice.resource.mode==='commands'?'Inline commands':'No build'}`);limiterConfirm.disabled=false;}
  catch(error){if(request!==limiterRequest)return;limiterCopyError(error.message);limiterPreview.textContent='Could not load this limiter.';}
 });
 limiterConfirm.addEventListener('click',()=>{if(!limiterChoice)return;$('[name="limiter"]').value=limiterChoice.invocation;$('[name="limiter"]').refreshShell?.();let card=$('#limiter-resources .resource');if(!card)card=addResource('limiter');fill(card,limiterChoice.resource);visibility();for(const input of card.querySelectorAll('[data-path]')){delete input.dataset.validated;if(input.value.trim())validatePath(input);}updateLimiterStatus();limiterDialog.close();});
 if(!limiterLoaded)addResource('limiter',editing?null:{root:`${repoRoot}/implementation/build/runsolver`,mode:'script',script:`${repoRoot}/implementation/build/runsolver/build.sh`,artifact:'runsolver',source:''});
 $('[name="axiom_mode"]').value=initial.axiom_globs?.length?'custom':'cluster';
 prefillExamples(form);
 const picker=$('#path-browser');let pickedInput=null,pickerDirectory='',pickedPathCallback=null;
 async function browse(directory,fallback=true){
  try{const data=await api('/api/browse-path',{directory});pickerDirectory=data.directory;$('#path-location').value=data.directory;const ancestors=$('#path-ancestors');ancestors.replaceChildren();let current=data.directory;while(true){const ancestor=current;ancestors.append(button(ancestor,()=>{closeAncestors();browse(ancestor);},'path-ancestor'));if(current==='/')break;current=current.replace(/\/[^/]+$/,'')||'/';}closeAncestors();const items=$('#path-items');items.replaceChildren();
   for(const item of data.items){const selectable=item.directory||pickedInput?.dataset.path==='file'||pickedInput?.dataset.path==='glob';const row=button(`${item.directory?'📁':'📄'}  ${item.name}`,()=>{if(item.directory)browse(item.path);else if(selectable){pickedInput.value=item.path;picker.close();if(pickedPathCallback){const callback=pickedPathCallback;pickedPathCallback=null;callback(item.path);}else validatePath(pickedInput);}},'path-item');row.disabled=!selectable;items.append(row);}
   text('#path-help',data.truncated?'Showing the first 1,000 entries (folders and archives first). Type a more specific directory above.':pickedInput?.dataset.path==='file'?'Open folders or choose a file.':pickedInput?.dataset.path==='glob'?'Choose a file or open a folder to select a path pattern.':'Open folders, then choose the current directory.');$('#path-parent').dataset.path=data.parent;
  }catch(error){if(fallback&&directory)browse('',false);else text('#path-help',error.message);}
 }
 function closeAncestors(){const menu=$('#path-ancestors');menu.hidden=true;$('#path-ancestors-toggle').setAttribute('aria-expanded','false');$('#path-location').setAttribute('aria-expanded','false');}
 function setupBrowsers(root){root.querySelectorAll('[data-path]').forEach(input=>{if(input.dataset.browserReady)return;input.dataset.browserReady='true';const choose=button('Browse…',()=>{pickedInput=input;const current=input.value.trim();let start=current;
   if(input.dataset.path==='glob'){
    if(/\.(?:zip|tar|tar\.gz|tgz|tar\.bz2|tbz2|tar\.xz|txz)$/i.test(current))start=current.replace(/\/[^/]*$/,'');
    const cuts=['*','?','['].map(mark=>current.indexOf(mark)).filter(index=>index>=0);
    const cut=cuts.length?Math.min(...cuts):current.length;
    if(cuts.length)start=current.slice(0,cut).replace(/\/$/,'');
   }
   if(input.dataset.path==='file')start=current.replace(/\/[^/]*$/,'');
   $('#path-select-directory').hidden=input.dataset.path==='file';picker.showModal();browse(start.startsWith('/')?start:'');},'secondary path-browse');
   const row=el('div',undefined,'path-input-row');input.replaceWith(row);row.append(input,choose);
 });}
 setupBrowsers(form);
 picker.addEventListener('close',()=>{pickedPathCallback=null;});
 picker.addEventListener('click',event=>{if(!event.target.closest('.path-combobox'))closeAncestors();});
 $('#path-parent').addEventListener('click',()=>browse($('#path-parent').dataset.path));$('#path-ancestors-toggle').addEventListener('click',()=>{const menu=$('#path-ancestors');menu.hidden=!menu.hidden;const expanded=String(!menu.hidden);$('#path-ancestors-toggle').setAttribute('aria-expanded',expanded);$('#path-location').setAttribute('aria-expanded',expanded);});$('#path-go').addEventListener('click',()=>{closeAncestors();browse($('#path-location').value);});$('#path-location').addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();closeAncestors();browse(event.target.value);}else if(event.key==='Escape')closeAncestors();});
 $('#path-select-directory').addEventListener('click',()=>{if(!pickedInput)return;const kind=pickedInput.dataset.path;if(kind==='file')return;pickedInput.value=pickerDirectory+(kind==='glob'?(pickedInput.name==='axiom_glob'?'/**/*.ax':'/**/*.p'):'');picker.close();validatePath(pickedInput);});
 $('#limiter-browse').addEventListener('click',()=>{const invocation=$('[name="limiter"]');const current=invocation.value.trim()||invocation.placeholder;const first=current.match(/^(?:'[^']*'|"[^"]*"|\S+)/)?.[0]||'';const binary=first.replace(/^['"]|['"]$/g,'');pickedInput={dataset:{path:'file'},value:binary};pickedPathCallback=path=>{const rest=current.slice(first.length).trimStart();const quoted="'"+path.replace(/'/g,"'\\''")+"'";invocation.value=quoted+(rest?' '+rest:'');invocation.refreshShell?.();updateLimiterStatus();};$('#path-select-directory').hidden=true;picker.showModal();browse(binary.startsWith('/')?binary.replace(/\/[^/]*$/,''):'');});
 // Repeated card fields are read from their card, never from a flattened form.
 function rootValue(name){return form.querySelector(`[name="${name}"]`).value;}
 function updateStepStates(){
  const readyPath=input=>input&&input.value.trim()&&input.dataset.validated===input.value;
  const cards=[...document.querySelectorAll('#configurations .configuration')];
  const names=cards.map(card=>card.querySelector('[name="solver_name"]').value.trim().toLocaleLowerCase());
  cards.forEach((card,index)=>{card.dataset.state=names[index]&&names.filter(name=>name===names[index]).length===1&&[...card.querySelectorAll('.solver-card-section')].every(section=>section.dataset.state==='valid')?'valid':'invalid';});
  const mode=rootValue('mode');
  const basics=['name','host','partition'].every(name=>form.querySelector(`[name="${name}"]`)?.validity.valid&&rootValue(name).trim());
  const solvers=mode==='interactive'?cards.length>0&&cards.every(card=>card.dataset.state==='valid'):
   mode==='jobpairs'?readyPath(form.querySelector('[name="jobpairs"]')):
   mode==='configurations'?readyPath(form.querySelector('[name="configurations_file"]')):false;
  const problems=['problem-selection','axiom-selection'].every(id=>$('#'+id)?.dataset.state==='valid');
  const limiter=['limiter-invocation-section','limiter-build-section'].every(id=>$('#'+id)?.dataset.state==='valid');
  const review=Number(rootValue('batch_size'))>0&&['problem-major','solver-major','random'].includes(rootValue('pair_order'));
  [basics,solvers,problems,limiter,review].forEach((valid,index)=>{const tab=$('#steps li:nth-child('+(index+1)+')');if(tab)tab.dataset.state=valid?'valid':'invalid';});
 }
 function visibility(){
  const mode=rootValue('mode');document.querySelectorAll('[data-mode]').forEach(n=>n.hidden=n.dataset.mode!==mode);const problemGlobs=$('#problem-glob-selection');if(problemGlobs)problemGlobs.hidden=mode==='jobpairs';else $('#problem-selection').hidden=mode==='jobpairs';
  $('#problems-from-jobpairs').hidden=mode!=='jobpairs';$('#custom-axioms').hidden=rootValue('axiom_mode')!=='custom';const clusterHelp=$('#cluster-axioms-help');if(clusterHelp)clusterHelp.hidden=rootValue('axiom_mode')==='custom';updateProblemStatus();
  const imported=['jobpairs','configurations'].includes(mode);$('#building-create').hidden=!imported;$('#imported-build-help').hidden=!imported;
  document.querySelectorAll('.resource').forEach(card=>{const choice=$('.resource-mode',card).value;card.querySelectorAll('[data-recipe]').forEach(n=>n.hidden=n.dataset.recipe!==choice);$('[data-resource-build]',card).hidden=choice==='none';});
  document.querySelectorAll('.configuration').forEach(card=>{const choice=$('.solver-build-mode',card).value;$('[data-solver-build]',card).hidden=choice==='none';updateSolverStatus(card);});updateLimiterStatus();updateStepStates();
 }
 form.addEventListener('change',visibility);visibility();
 function updateProblemStatus(){
  const ready=input=>input.value.trim()&&input.dataset.validated===input.value;
  const mode=rootValue('mode');const problems=[...document.querySelectorAll('[name="glob"]')];
  const inputsReady=mode==='jobpairs'||(problems.length>0&&problems.every(ready));
  const custom=rootValue('axiom_mode')==='custom';const axioms=[...document.querySelectorAll('[name="axiom_glob"]')];
  const axiomsReady=!custom||(axioms.length>0&&axioms.every(ready));
  for(const [id,valid,label] of [
   ['problem-selection',inputsReady,mode==='jobpairs'?'From jobpairs.csv':inputsReady?`${problems.length} source${problems.length===1?'':'s'} ready`:'Check problem sources'],
   ['axiom-selection',axiomsReady,custom?axiomsReady?`${axioms.length} axiom source${axioms.length===1?'':'s'} ready`:'Check axiom sources':'Cluster TPTP axioms']]){
   const section=$('#'+id),status=$('.solver-section-status',section);if(!status)continue;section.dataset.state=valid?'valid':'invalid';status.textContent=label;
  }
  updateStepStates();
 }
 form.addEventListener('input',event=>{if(event.target.matches('[name="glob"],[name="axiom_glob"]'))updateProblemStatus();});
 function updateLimiterStatus(){
  const invocationSection=$('#limiter-invocation-section'),buildSection=$('#limiter-build-section');if(!invocationSection||!buildSection)return;
  const pathReady=input=>input&&input.value.trim()&&input.dataset.validated===input.value;
  const command=rootValue('limiter').trim();
  const invocationReady=command&&!command.startsWith('/path/to/')&&command.includes('{{solver_command}}');
  const resources=[...$('#limiter-resources').querySelectorAll('.resource')];
  const buildReady=resources.length>0&&resources.every(card=>{
   const field=name=>card.querySelector(`[name="${name}"]`),choice=field('mode').value;
   if(!pathReady(field('root')))return false;
   if(choice==='none')return true;
   if(choice==='script'&&!pathReady(field('script')))return false;
   if(choice==='commands'&&!field('commands').value.trim())return false;
   return (!field('source').value.trim()||pathReady(field('source')))&&field('artifact').value.trim();
  });
  for(const [section,ready,good,bad] of [
   [invocationSection,invocationReady,'Invocation ready','Check limiter invocation'],
   [buildSection,buildReady,'Runtime/build ready','Check limiter root and build']]){
   section.dataset.state=ready?'valid':'invalid';$('.solver-section-status',section).textContent=ready?good:bad;
  }
  updateStepStates();
 }
 form.addEventListener('input',event=>{if(event.target.matches('[name="limiter"]')||event.target.closest('#limiter-resources'))updateLimiterStatus();});
 function updateSolverStatus(card){
  const value=name=>card.querySelector(`[name="${name}"]`).value.trim();
  const buildSelected=rootValue('building_mode')==='create'&&value('build_mode')==='script';
  const pathReady=name=>{const input=card.querySelector(`[name="${name}"]`);return input.value.trim()&&input.dataset.validated===input.value;};
  const root=value('solver_directory');
  const runtimeReady=root.startsWith('/')&&!root.includes('/path/to/')&&pathReady('solver_directory')&&
   value('command')&&value('command')!==card.querySelector('[name="command"]').placeholder&&
   !/^\.\/solver(?:\s|$)/.test(value('command'));
  const buildReady=!buildSelected||(pathReady('build_source')&&pathReady('build_script'));
  const positive=name=>{const input=card.querySelector(`[name="${name}"]`);return input.value.trim()!==''&&input.validity.valid&&Number(input.value)>0;};
  const limitsReady=['wc_limit','cpu_limit','cores'].every(positive)&&
   /^\s*(?:\d+(?:\.\d+)?)(?:\s*(?:[kmgpt]i?b?|b))?\s*$/i.test(value('mem_limit'))&&
   ['exclusive_cpu','exclusive_node'].every(name=>['true','false'].includes(value(name)));
  for(const [section,ready,good,bad] of [
   ['runtime',runtimeReady,'Runtime ready','Set root and command'],
   ['build',buildReady,buildSelected?'Build ready':'No build selected','Set source and build script'],
   ['limits',limitsReady,'Limits ready','Set limits and placement']]){
   const details=card.querySelector(`[data-solver-section="${section}"]`);details.dataset.state=ready?'valid':'invalid';$('.solver-section-status',details).textContent=ready?good:bad;
  }
  card.dataset.state=Boolean(value('solver_name'))&&[...card.querySelectorAll('.solver-card-section')].every(section=>section.dataset.state==='valid')?'valid':'invalid';
  updateStepStates();
 }
 form.addEventListener('input',event=>{const card=event.target.closest('.configuration');if(card){if(event.target.name==='solver_name')configurationTitle(card);updateSolverStatus(card);}});
 form.addEventListener('change',event=>{const card=event.target.closest('.configuration');if(card)updateSolverStatus(card);});
 async function validatePath(input){
  const status=$('.validation',input.closest('label'));if(!input.value.trim()){if(status)status.textContent='';const card=input.closest('.configuration');if(card)updateSolverStatus(card);return false;}
  const original=input.value;if(status){status.className='validation';status.textContent='Checking…';}
  try{const result=await api('/api/validate-path',{kind:input.dataset.path,role:input.name==='axiom_glob'?'axiom':'problem',value:original});if(input.value!==original)return false;if(status){status.className='validation valid';status.textContent=result.count?`${result.count} files matched · ${result.path}`:`Found · ${result.path}`;}input.dataset.validated=original;const card=input.closest('.configuration');if(card)updateSolverStatus(card);if(input.name==='glob'||input.name==='axiom_glob')updateProblemStatus();if(input.closest('#limiter-resources'))updateLimiterStatus();updateStepStates();return true;}
  catch(error){if(input.value===original&&status){status.className='validation invalid';status.textContent=error.message;}delete input.dataset.validated;const card=input.closest('.configuration');if(card)updateSolverStatus(card);if(input.name==='glob'||input.name==='axiom_glob')updateProblemStatus();if(input.closest('#limiter-resources'))updateLimiterStatus();updateStepStates();return false;}
 }
 form.addEventListener('focusout',event=>{if(event.target.matches('[data-path]'))validatePath(event.target);});
 for(const card of document.querySelectorAll('.configuration'))for(const input of card.querySelectorAll('[data-path]')){
  if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 }
 for(const input of document.querySelectorAll('[name="glob"],[name="axiom_glob"]'))if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 for(const input of document.querySelectorAll('#limiter-resources [data-path]'))if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 function fields(card){return Object.fromEntries([...card.querySelectorAll('[name]')].map(n=>[n.name,n.value]));}
 function payload(){const names=['name','host','partition','batch_size','pair_order','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter','axiom_mode'];const data=Object.fromEntries(names.map(n=>[n,rootValue(n)]));const cards=[...document.querySelectorAll('.configuration')];data.configurations=cards.map(card=>{const row=fields(card);for(const key of Object.keys(row))if(key.startsWith('build_'))delete row[key];row.cpus='auto';return row;});data.resources=[...document.querySelectorAll('.resource')].sort((a,b)=>Number(a.dataset.resourceOrder)-Number(b.dataset.resourceOrder)).filter(node=>data.mode!=='interactive'||node.dataset.resourceRole==='limiter').map(node=>({...fields(node),role:node.dataset.resourceRole}));if(data.mode==='interactive'&&data.building_mode==='create'){const seen=new Map();for(const card of cards){const value=fields(card);const resource={role:'solver',root:value.solver_directory,mode:value.build_mode,source:value.build_source,script:value.build_script,artifact:value.build_mode==='script'?solverArtifact(value.command,value.solver_directory):''};const previous=seen.get(resource.root);if(previous){if(resource.mode!=='none'&&previous.mode!=='none'&&JSON.stringify(resource)!==JSON.stringify(previous))throw new Error(`Conflicting build settings for solver root ${resource.root}`);if(previous.mode==='none'&&resource.mode!=='none')Object.assign(previous,resource);}else{seen.set(resource.root,resource);data.resources.push(resource);}}}data.globs=[...document.querySelectorAll('[name="glob"]')].map(n=>n.value);data.axiom_globs=data.axiom_mode==='custom'?[...document.querySelectorAll('[name="axiom_glob"]')].map(n=>n.value):[];return data;}
 function solverArtifact(command,root){const executable=command.trim().match(/^(?:'([^']+)'|"([^"]+)"|(\S+))/)?.slice(1).find(Boolean)||'';if(!executable)throw new Error('Enter the solver command before configuring its build.');const path=executable.startsWith(root+'/')?executable.slice(root.length+1):executable.replace(/^\.\//,'');if(path.startsWith('/')||path==='..'||path.startsWith('../')||path.includes('/../')||path.includes('{{'))throw new Error('For a remote build, start the solver command with an executable relative to the solver root.');return path;}
 function renderStep(){sections.forEach((s,i)=>s.hidden=i!==step);document.querySelectorAll('#steps li').forEach((n,i)=>{n.classList.toggle('current',i===step);const tab=$('button',n);if(i===step)tab.setAttribute('aria-current','step');else tab.removeAttribute('aria-current');});$('#builder-back').hidden=step===0;$('#builder-next').hidden=step===4;$('#builder-save').hidden=step!==4;text('#step-count',`Step ${step+1} of 5`);$('#builder-error').hidden=true;}
 function showError(error){text('#builder-error',error.message);$('#builder-error').hidden=false;}
 async function validSection(){
  if(step===1&&rootValue('mode')==='interactive'&&!$('#configurations .configuration'))throw new Error('Add a solver configuration or copy one from an existing workflow before continuing.');
  const inputs=[...sections[step].querySelectorAll('input,textarea,select')].filter(n=>n.type!=='hidden'&&!n.closest('[hidden]'));
  for(const input of inputs){if(!input.value.trim()&&!input.dataset.optional){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;input.focus();throw new Error('Fill in each visible setting before continuing.');}if(!input.checkValidity()){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;input.reportValidity();throw new Error('Please correct the highlighted field.');}}
  for(const input of inputs.filter(n=>n.dataset.path&&n.value.trim())){if(input.dataset.validated!==input.value&&!await validatePath(input)){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;throw new Error('Correct the path highlighted above.');}}
  if(step===1&&rootValue('mode')==='interactive')for(const card of document.querySelectorAll('.configuration')){
   updateSolverStatus(card);const section=card.querySelector('.solver-card-section[data-state="invalid"]');if(section&&!section.hidden){section.open=true;section.querySelector('input,textarea,select')?.focus();throw new Error('Complete the highlighted solver section before continuing.');}
  }
  if(step===1&&rootValue('mode')==='interactive'&&[...document.querySelectorAll('#configurations .configuration')].some(card=>card.dataset.state==='invalid'))throw new Error('Give each solver configuration a distinct name.');
  if(step===2){updateProblemStatus();const section=sections[step].querySelector('.problem-card-section[data-state="invalid"]');if(section){section.open=true;throw new Error('Complete the highlighted problem or axiom section before continuing.');}}
  if(step===3){updateLimiterStatus();const section=sections[step].querySelector('.limiter-card-section[data-state="invalid"]');if(section){section.open=true;throw new Error('Complete the highlighted limiter section before continuing.');}}
 }
 const fileDescriptions={'jobpairs.csv':'One row per solver call and problem, with its limits and placement requirements.','configurations.csv':'The reusable solver configurations from which the call list is generated.','problem-globs.txt':'The problem paths, globs, or archives used to generate calls.','axiom-globs.txt':'Custom TPTP axiom files to package for include statements.','building.txt':'Runtime roots, source roots, build scripts, and expected executables.','resource_limiter_template.txt':'The command template that wraps each solver call.','Makefile':'Prepare, build, submit, sync, and monitor commands for this workflow.'};
 async function prepareReview(){if(!rootValue('batch_size'))form.querySelector('[name="batch_size"]').value='1';updateStepStates();reviewed=payload();const query=editing?'?'+params({directory:context.dataset.directory}):'';const data=await api('/api/preview'+query,reviewed);text('#review-summary',`${data.count} explicit calls → ${data.directory}`);const files=$('#review-files');files.replaceChildren();for(const [name,body] of Object.entries(data.files)){if(name.startsWith('.'))continue;const details=el('details'),pre=el('pre');if(name==='resource_limiter_template.txt'||name==='Makefile'||name.endsWith('.sh'))highlightShell(pre,body);else pre.textContent=body;details.append(el('summary',name),el('p',fileDescriptions[name]||'Remote build recipe used when submitting this workflow.'),pre);files.append(details);}}
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
 form.addEventListener('input',updateStepStates);form.addEventListener('change',updateStepStates);
 form.addEventListener('submit',e=>e.preventDefault());renderStep();updateStepStates();
}
