'use strict';
if(document.body.dataset.page==='new'){
 const form=$('#builder');let step=0;const initial=JSON.parse($('#builder-initial').textContent||'{}');const context=$('#builder-context');const editing=context?.dataset.editing==='true';
 const repoRoot=document.body.dataset.repoRoot;
 let coreTopologyState=null;
 if(!editing&&!initial.mode)initial.mode='interactive';
 const sections=[...form.querySelectorAll('[data-step]')];
 const pendingWork=new Map();let workTimer=null;
 function showWork(){const panel=$('#builder-progress');if(!pendingWork.size){panel.hidden=true;if(workTimer){clearInterval(workTimer);workTimer=null;}return;}const latest=[...pendingWork.values()].at(-1);panel.hidden=false;if($('#builder-progress-message').textContent!==latest.message)text('#builder-progress-message',latest.message);const bar=$('#builder-progress-bar'),detail=$('#builder-progress-detail');bar.hidden=!latest.total;if(latest.total){bar.value=Math.min(100,latest.completed/latest.total*100);detail.textContent=latest.unit==='bytes'?`${memory(latest.completed)} / ${memory(latest.total)} · ${Math.floor(bar.value)}%`:`${latest.completed.toLocaleString()} / ${latest.total.toLocaleString()} · ${Math.floor(bar.value)}%`;}else detail.textContent=latest.detail||'';text('#builder-progress-time',`${Math.floor((Date.now()-latest.started)/1000)}s elapsed`);if(!workTimer)workTimer=setInterval(showWork,1000);}
 function beginWork(message){const key=Symbol();pendingWork.set(key,{message,started:Date.now()});showWork();const finish=()=>{pendingWork.delete(key);showWork();};finish.update=event=>{const work=pendingWork.get(key);if(!work)return;if(event.phase==='extracting'||event.phase==='extracted'){work.message=`${event.phase==='extracted'?'Extracted':'Extracting'} ${event.path.split('/').pop()}…`;work.completed=event.completed;work.total=event.total;work.unit='bytes';}else if(event.phase==='scanning'||event.phase==='indexed'){work.message=`${event.phase==='indexed'?'Indexed':'Scanning'} ${event.path.split('/').pop()}…`;work.completed=null;work.total=null;work.detail=`${event.completed.toLocaleString()} files found`;}else if(['calls','batches','paths'].includes(event.phase)){work.message=event.path+'…';work.completed=event.completed;work.total=event.total;work.unit='items';}else{work.message=event.path;work.completed=null;work.total=null;work.detail='';}showWork();};return finish;}
 let activeBuilderOperation=null;
 $('#builder-cancel').addEventListener('click',async()=>{if(!activeBuilderOperation)return;const cancel=$('#builder-cancel');cancel.disabled=true;try{const response=await api('/api/builder/cancel',{id:activeBuilderOperation});if(response.cancelled){text('#builder-progress-message','Cancelling workflow preparation…');}else{cancel.hidden=true;text('#builder-progress-message','Finishing workflow files; cancellation is no longer available…');}}catch(error){cancel.disabled=false;showError(error);}});
 async function streamBuilder(path,data,onProgress){
  const response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:JSON.stringify(data)});
  if(!response.ok){const value=await response.json();throw new Error(value.error||`Request failed (${response.status})`);}
  if(!response.body)throw new Error('This browser cannot display live build progress.');
  const reader=response.body.getReader(),decoder=new TextDecoder();let buffer='',operationId=null;
  try{while(true){const {value,done}=await reader.read();buffer+=decoder.decode(value||new Uint8Array(),{stream:!done});let newline;
   while((newline=buffer.indexOf('\n'))>=0){const line=buffer.slice(0,newline);buffer=buffer.slice(newline+1);if(!line)continue;const event=JSON.parse(line);
    if(event.type==='started'){operationId=event.id;activeBuilderOperation=event.id;$('#builder-cancel').hidden=false;$('#builder-cancel').disabled=false;}
    else if(event.type==='progress'){onProgress(event);if(event.phase==='committing')$('#builder-cancel').hidden=true;}
    else if(event.type==='cancelled'){const error=new Error('Workflow preparation cancelled.');error.cancelled=true;throw error;}
    else if(event.type==='error')throw new Error(event.error);
    else if(event.type==='result')return event;
   }
   if(done)break;
  }}finally{if(activeBuilderOperation===operationId){activeBuilderOperation=null;$('#builder-cancel').hidden=true;}reader.cancel().catch(()=>{});}
  throw new Error('The progress stream ended before the workflow operation completed.');
 }
 function prefillExamples(root){root.querySelectorAll('input[placeholder],textarea[placeholder]').forEach(input=>{if(!input.value && !['glob','account'].includes(input.name))input.value=input.placeholder;});}
 function add(template,target){const node=$('#'+template).content.firstElementChild.cloneNode(true);node.querySelector('.add-name-files')?.addEventListener('click',()=>node.querySelector('.problem-name-files').click());prefillExamples(node);if(node.matches('.configuration'))node.querySelector('[name="solver_name"]').value='';$('.remove',node).addEventListener('click',()=>{node.remove();if(target==='globs'||target==='axiom-globs')updateProblemStatus();if(target==='limiter-resources')updateLimiterStatus();updateStepStates();});$('#'+target).append(node);setupBrowsers(node);shellEditors(node);node.querySelectorAll('textarea[name="postprocessor_command"]').forEach(shellEditor);window.initCollapsibles?.();if(node.matches('.configuration')){updateSolverStatus(node);queueMicrotask(()=>checkThreads(node.querySelector('[name="threads_per_core"]')));}if(target==='globs'||target==='axiom-globs')updateProblemStatus();if(target==='limiter-resources')updateLimiterStatus();updateStepStates();return node;}
 $('#add-configuration').addEventListener('click',()=>{const card=add('configuration-template','configurations');$('#builder-error').hidden=true;visibility();card.scrollIntoView({behavior:'smooth',block:'start'});});
 $('#add-glob').addEventListener('click',()=>add('glob-template','globs'));
 $('#add-axiom-glob').addEventListener('click',()=>add('axiom-glob-template','axiom-globs'));
 let resourceOrder=0;
 function addResource(role,values){
  if(role==='limiter'&&values?.mode==='none'&&values.root===`${repoRoot}/implementation/build/runsolver`){values={...values,script:values.script||`${repoRoot}/implementation/build/runsolver/build.sh`,source:values.source||`${repoRoot}/implementation/build/runsolver/source`,artifact:values.artifact||'runsolver'};}
  const node=add('resource-template',role==='limiter'?'limiter-resources':'solver-resources');node.dataset.resourceRole=role;node.dataset.resourceOrder=resourceOrder++;const caption=$('h3',node);const toggle=$('.collapse-toggle',caption);caption.textContent=role==='limiter'?'Limiter Resource':'Cluster Build for Imported Solver';if(toggle){toggle.setAttribute('aria-label',`Collapse ${caption.textContent}`);caption.prepend(toggle);}if(role==='solver')node.querySelector('[name="artifact"]').placeholder='solver';else $('.remove',node).hidden=true;fill(node,values);if(role==='limiter'){
  const option=$('[data-limiter-build-option]',node),checkbox=$('[data-limiter-build-toggle]',node),mode=$('.resource-mode',node);
  option.hidden=false;
  checkbox.addEventListener('change',()=>{if(checkbox.checked){mode.value=node.dataset.lastBuildMode||'script';}else{if(mode.value!=='none')node.dataset.lastBuildMode=mode.value;mode.value='none';}});
  updateLimiterStatus();
 }return node;}
 $('#add-solver-resource').addEventListener('click',()=>{addResource('solver',{mode:'script',artifact:''});visibility();});
 function configurationTitle(card){const name=card.querySelector('[name="solver_name"]').value.trim(),caption=$('h3',card);const toggle=$('.collapse-toggle',caption);caption.textContent=name?`${name} Configuration`:'Solver Configuration';if(toggle){toggle.setAttribute('aria-label',`${card.classList.contains('collapsed')?'Expand':'Collapse'} ${caption.textContent}`);caption.prepend(toggle);}}
 function nameFileContents(files){return files.map(file=>file.content).join('\n');}
 function renderNameFiles(row){
  const list=row.querySelector('.problem-name-file-list');if(!list)return;
  list.replaceChildren();
  for(const [index,file] of (row._nameFiles||[]).entries()){
   const item=document.createElement('li'),name=document.createElement('span'),remove=document.createElement('button');
   name.textContent=file.name;name.title=file.name;remove.type='button';remove.className='secondary remove-name-file';remove.textContent='×';remove.title=`Remove ${file.name}`;remove.setAttribute('aria-label',`Remove ${file.name}`);
   remove.addEventListener('click',async()=>{
    row.querySelector('.problem-name-files')._request=null;row.querySelector('.problem-name-files')._reading=false;row._nameFiles.splice(index,1);renderNameFiles(row);
    const text=row.querySelector('[name="problem_selection"]');text.value=nameFileContents(row._nameFiles);row._filesDraft=text.value;
    const picker=row.querySelector('.problem-name-files');picker.value='';picker.setCustomValidity(row._nameFiles.length?'':'Choose one or more files of problem names.');
    row.querySelector('.problem-names-status').textContent=row._nameFiles.length?`${row._nameFiles.length} name ${row._nameFiles.length===1?'file':'files'} selected.`:'No files selected.';
    text.dispatchEvent(new Event('input',{bubbles:true}));await validatePath(row.querySelector('[name="glob"]'));
   });
   item.append(name,remove);list.append(item);
  }
 }
 function initializeProblemNames(row,values){
  const mode=row.querySelector('.problem-names-mode');if(!mode)return;
  const text=row.querySelector('[name="problem_selection"]');
  mode.value=values?.problem_selection_mode|| (text.value.trim()?'names':'disabled');
  row._namesMode=mode.value;row._namesDraft=mode.value==='names'?text.value:'';row._filesDraft=mode.value==='files'?text.value:'';
  row.querySelector('.problem-names-files').hidden=mode.value!=='files';row.querySelector('.problem-names-text').hidden=mode.value!=='names';
  if(mode.value==='disabled')text.value='';
  row._nameFiles=Array.isArray(values?.problem_selection_files)?values.problem_selection_files.filter(file=>typeof file?.name==='string'&&typeof file?.content==='string'):[];
  if(mode.value==='files'&&nameFileContents(row._nameFiles)!==text.value)row._nameFiles=text.value?[{name:'Saved names',content:text.value}]:[];
  renderNameFiles(row);
  if(mode.value==='files')row.querySelector('.problem-names-status').textContent=row._nameFiles.length?`${row._nameFiles.length} name ${row._nameFiles.length===1?'file':'files'} selected.`:'No name files added yet.';
  mode.refreshShell?.();
 }
 function fill(node,values){for(const [name,value] of Object.entries(values||{})){const field=node.querySelector(`[name="${name}"]`);if(field){if(field.type==='checkbox')field.checked=value===true;else if(name==='cores')setCoreRequest(field,value??'1');else field.value=name==='problem_selection'&&Array.isArray(value)?value.join('\n'):name.startsWith('omit_')?String(value||false).toLowerCase():(value??'');field.refreshShell?.();}}if(node.matches('.glob-row'))initializeProblemNames(node,values);if(node.matches('.configuration')){configurationTitle(node);updateSolverStatus(node);queueMicrotask(()=>validateSolverCommand(node));queueMicrotask(()=>validatePostprocessor(node));queueMicrotask(()=>validateEnvironment(node));queueMicrotask(()=>checkThreads(node.querySelector('[name="threads_per_core"]')));}}
 fill($('#workflow-postprocessing'),Object.fromEntries(Object.entries(initial.postprocessor||{mode:'default',timeout:30}).map(([key,value])=>['workflow_postprocessor_'+key,value])));
 for(const name of ['name','partition','account','reservation','batch_size','max_outstanding','pair_order','mode','jobpairs','configurations_file','limiter_mode','limiter_file','limiter']){const field=form.querySelector(`[name="${name}"]`);if(field&&initial[name]!==undefined)field.value=initial[name];}
 $('[name="get_computer_info"]').checked=initial.get_computer_info!==false;
 const accountField=$('#slurm-account-field'),accountSelect=form.querySelector('[name="account"]'),accountLoading=$('#slurm-account-loading'),accountStatus=$('#slurm-account-status'),accountRetry=$('#slurm-account-retry');
 if(initial.account){accountSelect.add(new Option(initial.account,initial.account));accountSelect.value=initial.account;}
 let accountsReady=false;
 async function loadAccounts(){
  accountsReady=false;accountRetry.hidden=true;accountLoading.hidden=false;accountLoading.textContent='Checking SLURM account memberships…';accountSelect.disabled=true;
  try{
   const result=await api('/api/slurm-accounts?'+params({host:rootValue('host')}));
   const requested=accountSelect.dataset.loaded?accountSelect.value:(editing?initial.account:undefined);
   accountSelect.replaceChildren();
   for(const name of result.accounts)accountSelect.add(new Option(name,name));
   const selected=result.accounts.includes(requested)?requested:result.preferred;
   accountSelect.value=selected;accountSelect.dataset.loaded='true';
   accountField.hidden=result.accounts.length===1;
   accountStatus.textContent=(requested&&requested!==selected?`“${requested}” is unavailable for this cluster user; using ${selected||'the cluster default'}. `:'')+'Accounts available to your cluster user. '+(result.cached?'Checked within the last five minutes.':'Memberships just checked.');
   accountLoading.hidden=true;accountsReady=true;
  }catch(error){accountField.hidden=true;accountLoading.textContent=error.message+' You can keep editing; the saved account (or cluster default) will be used until memberships are checked.';accountRetry.hidden=false;}
  finally{accountSelect.disabled=false;updateStepStates();}
 }
 accountRetry.addEventListener('click',loadAccounts);
 loadAccounts();

 $('[name="limiter_mode"]').value='inline';$('[name="limiter_file"]').value='';
 if(!editing&&!initial.limiter){$('[name="limiter_mode"]').value='inline';$('[name="limiter"]').value=`${repoRoot}/implementation/build/runsolver/runsolver --cpu-limit {{cpu_limit}} --wall-clock-limit {{wc_limit}} --rss-swap-limit {{mem_limit_mib}} --timestamp --add-eof --watcher-data {{watcher_log}} --var {{var_file}} --solver-data {{solver_log}} {{solver_command}}`;}
 for(const values of initial.configurations||[]){const card=add('configuration-template','configurations');fill(card,{...values,solver_name:values.solver_name||values.command?.trim().split(/\s+/)[0]?.split('/').pop()||''});const resource=(initial.resources||[]).find(item=>item.role==='solver'&&item.root===values.solver_directory);if(resource)fill(card,{build_mode:resource.mode,build_source:resource.source,build_script:resource.script,build_policy:resource.build_policy||'always'});}
 const globs=initial.globs?.length?initial.globs:[];for(const [index,value] of globs.entries())fill(add('glob-template','globs'),value===null?null:{glob:value,problem_selection:initial.problem_selections?.[index]||'',problem_selection_mode:initial.problem_selection_modes?.[index],problem_selection_files:initial.problem_selection_files?.[index]});
 for(const value of initial.axiom_globs?.length?initial.axiom_globs:[`tptp://${document.body.dataset.tptpVersion}/Axioms/**/*.ax`])fill(add('axiom-glob-template','axiom-globs'),{axiom_glob:value});
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
 copySolver.addEventListener('change',()=>{const choice=copySolver.value?copyChoices[Number(copySolver.value)]:null;copyConfirm.disabled=!choice;if(choice)highlightShell(copyPreview,`${choice.command}\nRoot: ${choice.solver_directory}\nLimits: ${choice.cpu_limit}s CPU, ${choice.wc_limit}s wall, ${choice.mem_limit}, ${choice.cores} core(s), ${choice.threads_per_core||1} thread(s)/core\nBuild: ${choice.build_mode==='script'?choice.build_script:'none'}`);else copyPreview.textContent='Choose a solver configuration to preview it.';});
 copyConfirm.addEventListener('click',()=>{const choice=copyChoices[Number(copySolver.value)];if(!copySolver.value||!choice)return;const card=add('configuration-template','configurations');fill(card,choice);$('#builder-error').hidden=true;visibility();copyDialog.close();card.scrollIntoView({behavior:'smooth',block:'center'});for(const input of card.querySelectorAll('[data-path]'))if(input.value.trim())validatePath(input);});
 for(const [plural,singular,target,template,field] of [
  ['problems','problem','globs','glob-template','glob'],
  ['axioms','axiom','axiom-globs','axiom-glob-template','axiom_glob']]){
  const dialog=$(`#copy-${plural}-dialog`),workflow=$(`#copy-${plural}-workflow`),confirm=$(`#confirm-copy-${plural}`);
  const trigger=$(`#copy-${plural}`);
  if(!dialog||!workflow||!confirm||!trigger)continue;
  let choices=[],requestId=0;
  const error=message=>{text(`#copy-${plural}-error`,message);$(`#copy-${plural}-error`).hidden=!message;};
  const preview=message=>text(`#copy-${plural}-preview`,message);
  $(`#close-copy-${plural}`).addEventListener('click',()=>dialog.close());
  dialog.addEventListener('close',()=>{++requestId;});
  trigger.addEventListener('click',async()=>{
   const request=++requestId;choices=[];confirm.disabled=true;error('');preview(`Select a workflow to see its ${singular} sources.`);
   dialog.showModal();workflow.disabled=true;workflow.replaceChildren(el('option','Loading workflows…'));
   try{const data=await api('/api/solver-configurations?'+params({}));if(request!==requestId)return;workflow.replaceChildren(el('option','Choose a workflow…'));workflow.firstChild.value='';for(const item of data.workflows){const option=el('option',`${item.group} · ${item.name}`);option.value=item.directory;workflow.append(option);}workflow.disabled=false;}
   catch(failure){if(request===requestId)error(failure.message);}
  });
  workflow.addEventListener('change',async()=>{
   const request=++requestId;choices=[];confirm.disabled=true;error('');
   if(!workflow.value){preview(`Select a workflow to see its ${singular} sources.`);return;}
   preview(`Loading ${singular} sources…`);
   try{const data=await api(`/api/${singular}-sources?`+params({directory:workflow.value}));if(request!==requestId)return;choices=(data.globs||[]).map((glob,index)=>({glob,selection:data.problem_selections?.[index]||'',selectionMode:data.problem_selection_modes?.[index]}));preview(choices.map(item=>item.glob).join('\n')||`This workflow has no ${singular} sources.`);confirm.disabled=!choices.length;}
   catch(failure){if(request===requestId)error(failure.message);}
  });
  confirm.addEventListener('click',()=>{
   const sourceKey=(glob,selection)=>singular==='problem'?JSON.stringify([glob,selection||'']):glob;
   const existing=new Set([...document.querySelectorAll(`[name="${field}"]`)].map(input=>sourceKey(input.value.trim(),input.closest('.glob-row').querySelector('[name="problem_selection"]')?.value)));
   for(const item of choices){const glob=item.glob,key=sourceKey(glob,item.selection);if(!existing.has(key)){const row=add(template,target);fill(row,{[field]:glob,problem_selection:item.selection,problem_selection_mode:item.selectionMode});existing.add(key);validatePath($(`[name="${field}"]`,row));}}
   dialog.close();updateProblemStatus();
  });
 }
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
 if(!limiterLoaded)addResource('limiter',editing?null:{root:`${repoRoot}/implementation/build/runsolver`,mode:'none',build_policy:'changed',script:`${repoRoot}/implementation/build/runsolver/build.sh`,artifact:'runsolver',source:`${repoRoot}/implementation/build/runsolver/source`});
 prefillExamples(form);
 const picker=$('#path-browser'),browserLocationKey='slurmy-last-browser-directory';let pickedInput=null,pickerDirectory='',pickedPathCallback=null,browseRequest=0,remoteBrowser=false,pendingDirectory=null;
 let localDirectory='';const remoteDirectories=new Map();
 try{localDirectory=localStorage.getItem(browserLocationKey)||'';}catch{}
 if(!localDirectory.startsWith('/'))localDirectory='';
 async function browse(directory,fallback=true){
  if(directory===pendingDirectory)return;pendingDirectory=directory;
  const request=++browseRequest;const remote=remoteBrowser,selectedHost=rootValue('host');
  $('#path-items').replaceChildren();$('#path-select-directory').disabled=true;$('#path-parent').disabled=true;text('#path-help',remote?'Loading cluster directories…':'Loading local directories…');
  $('#path-browser h2').textContent=remoteBrowser?`Choose Cluster TPTP ${pickedInput?.name==='axiom_glob'?'Axioms':'Problems'} (${rootValue('host')})`:'Choose a Local Path';$('#path-select-directory').textContent=remoteBrowser?'Use this directory as a glob':'Choose this directory';
  try{if(!remote&&directory.startsWith('tptp://'))throw new Error('This browser shows local files. Use Browse cluster TPTP for tptp:// paths.');
   const data=await api(remote?'/api/browse-tptp':'/api/browse-path',{directory,host:selectedHost});if(request!==browseRequest)return;pickerDirectory=data.directory;if(remote)remoteDirectories.set(selectedHost+':'+pickedInput?.name,pickerDirectory);else{localDirectory=pickerDirectory;try{localStorage.setItem(browserLocationKey,localDirectory);}catch{}}$('#path-select-directory').disabled=remote&&pickerDirectory==='tptp://';$('#path-parent').disabled=false;$('#path-location').value=data.directory;const ancestors=$('#path-ancestors');ancestors.replaceChildren();let current=data.directory;while(true){const ancestor=current;ancestors.append(button(ancestor,()=>{closeAncestors();browse(ancestor);},'path-ancestor'));if(current===(remoteBrowser?'tptp://':'/'))break;current=remoteBrowser?(current.slice(8).includes('/')?current.replace(/\/[^/]+$/,''):'tptp://'):(current.replace(/\/[^/]+$/,'')||'/');}closeAncestors();const items=$('#path-items');items.replaceChildren();
   for(const item of data.items){const selectable=item.directory||['file','glob','build-script'].includes(pickedInput?.dataset.path);const row=button(`${item.directory?'📁':'📄'}  ${item.name}`,()=>{if(item.directory)browse(item.path);else if(selectable){if(pickedInput.dataset.path==='build-script'){const first=pickedInput.value.match(/^(?:'[^']*'|"[^"]*"|\S+)/)?.[0]||'';const rest=pickedInput.value.slice(first.length);pickedInput.value="'"+item.path.replace(/'/g,"'\\''")+"'"+rest;}else pickedInput.value=item.path;picker.close();if(pickedPathCallback){const callback=pickedPathCallback;pickedPathCallback=null;callback(item.path);}else validatePath(pickedInput);}},'path-item');row.disabled=!selectable;items.append(row);}
   text('#path-help',data.truncated?'Showing the first 1,000 entries (folders and archives first). Type a more specific directory above.':pickedInput?.dataset.path==='file'?'Open folders or choose a file.':pickedInput?.dataset.path==='glob'?'Choose a file or open a folder to select a path pattern.':'Open folders, then choose the current directory.');$('#path-parent').dataset.path=data.parent;
  }catch(error){if(request!==browseRequest)return;if(fallback&&directory)browse(remoteBrowser?'tptp://':'',false);else text('#path-help',error.message);}
  finally{if(request===browseRequest)pendingDirectory=null;}
 }
 function closeAncestors(){const menu=$('#path-ancestors');menu.hidden=true;$('#path-ancestors-toggle').setAttribute('aria-expanded','false');$('#path-location').setAttribute('aria-expanded','false');}
 function setupBrowsers(root){root.querySelectorAll('[data-path]').forEach(input=>{if(input.dataset.browserReady)return;input.dataset.browserReady='true';const choose=button('Browse…',()=>{pickedInput=input;const current=input.value.trim();remoteBrowser=false;let start=current.startsWith('tptp://')?'':current;
   if(input.dataset.path==='glob'){
    if(/\.(?:p|tptp|zip|tar|tar\.gz|tgz|tar\.bz2|tbz2|tar\.xz|txz)$/i.test(current))start=current.replace(/\/[^/]*$/,'');
    const cuts=['*','?','['].map(mark=>current.indexOf(mark)).filter(index=>index>=0);
    const cut=cuts.length?Math.min(...cuts):current.length;
    if(cuts.length)start=current.slice(0,cut).replace(/\/$/,'');
   }
   if(input.dataset.path==='build-script'){const word=current.match(/^(?:'([^']*)'|"([^"]*)"|(\S+))/);start=(word?.slice(1).find(Boolean)||'').replace(/\/[^/]*$/,'');}
   if(input.dataset.path==='file')start=current.replace(/\/[^/]*$/,'');
   $('#path-select-directory').hidden=['file','build-script'].includes(input.dataset.path);picker.showModal();browse((current!==input.placeholder&&start.startsWith('/')?start:'')||localDirectory);},'secondary path-browse');
   const row=el('div',undefined,'path-input-row');input.replaceWith(row);row.append(input,choose);if(['glob','axiom_glob'].includes(input.name))row.append(button('Browse cluster TPTP…',()=>{pickedInput=input;remoteBrowser=true;$('#path-select-directory').hidden=false;picker.showModal();const value=input.value.trim();const prefix=value.startsWith('tptp://')?value.split(/[?*\[]/,1)[0].replace(/\/$/,''):'';const start=prefix&&prefix!=='tptp:/'?(prefix.endsWith('.p')||prefix.endsWith('.tptp')||prefix.endsWith('.ax')?prefix.replace(/\/[^/]*$/,''):prefix):'';browse(start||remoteDirectories.get(rootValue('host')+':'+input.name)||`tptp://${document.body.dataset.tptpVersion}/${input.name==='axiom_glob'?'Axioms':'Problems'}`);},'secondary path-browse'));
 });}
 setupBrowsers(form);
 picker.addEventListener('close',()=>{++browseRequest;pendingDirectory=null;pickedPathCallback=null;});
 picker.addEventListener('click',event=>{if(!event.target.closest('.path-combobox'))closeAncestors();});
 $('#path-parent').addEventListener('click',()=>browse($('#path-parent').dataset.path));$('#path-ancestors-toggle').addEventListener('click',()=>{const menu=$('#path-ancestors');menu.hidden=!menu.hidden;const expanded=String(!menu.hidden);$('#path-ancestors-toggle').setAttribute('aria-expanded',expanded);$('#path-location').setAttribute('aria-expanded',expanded);});$('#path-location').addEventListener('blur',event=>{if(picker.open&&event.target.value.trim()!==pickerDirectory){closeAncestors();browse(event.target.value.trim(),false);}});$('#path-location').addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();closeAncestors();browse(event.target.value,false);}else if(event.key==='Escape')closeAncestors();});
 $('#path-select-directory').addEventListener('click',()=>{if(!pickedInput)return;const kind=pickedInput.dataset.path;if(kind==='file')return;pickedInput.value=(pickerDirectory==='/'?'':pickerDirectory.replace(/\/$/,''))+(kind==='glob'?(pickedInput.name==='axiom_glob'?'/**/*.ax':'/**/*.p'):'');picker.close();validatePath(pickedInput);});
 $('#limiter-browse').addEventListener('click',()=>{const invocation=$('[name="limiter"]');const current=invocation.value.trim()||invocation.placeholder;const first=current.match(/^(?:'[^']*'|"[^"]*"|\S+)/)?.[0]||'';remoteBrowser=false;const binary=first.replace(/^['"]|['"]$/g,'');pickedInput={dataset:{path:'file'},value:binary};pickedPathCallback=path=>{const rest=current.slice(first.length).trimStart();const quoted="'"+path.replace(/'/g,"'\\''")+"'";invocation.value=quoted+(rest?' '+rest:'');invocation.refreshShell?.();updateLimiterStatus();};$('#path-select-directory').hidden=true;picker.showModal();browse((binary.startsWith('/')?binary.replace(/\/[^/]*$/,''):'')||localDirectory);});
 // Repeated card fields are read from their card, never from a flattened form.
 function rootValue(name){return name==='host'?document.body.dataset.host:form.querySelector(`[name="${name}"]`).value;}
 function updateStepStates(){
  syncGlobalPostprocessor();
  const readyPath=input=>input&&input.value.trim()&&input.dataset.validated===input.value;
  const cards=[...document.querySelectorAll('#configurations .configuration')];
  const names=cards.map(card=>card.querySelector('[name="solver_name"]').value.trim().toLocaleLowerCase());
  cards.forEach((card,index)=>{card.dataset.state=names[index]&&names.filter(name=>name===names[index]).length===1&&[...card.querySelectorAll('.solver-card-section')].every(section=>section.dataset.state==='valid')?'valid':'invalid';});
  const mode=rootValue('mode');
  const basics=['name','partition'].every(name=>form.querySelector(`[name="${name}"]`)?.validity.valid&&rootValue(name).trim());
  const solvers=mode==='interactive'?$('#workflow-postprocessing').dataset.state==='valid'&&cards.length>0&&cards.every(card=>card.dataset.state==='valid'):
   mode==='jobpairs'?readyPath(form.querySelector('[name="jobpairs"]')):
   mode==='configurations'?readyPath(form.querySelector('[name="configurations_file"]')):false;
  const problems=['problem-selection','axiom-selection'].every(id=>$('#'+id)?.dataset.state==='valid');
  const limiter=['limiter-invocation-section','limiter-build-section'].every(id=>$('#'+id)?.dataset.state==='valid');
  const review=Number(rootValue('batch_size'))>0&&Number(rootValue('max_outstanding'))>0&&['problem-major','solver-major','random'].includes(rootValue('pair_order'));
  [basics,solvers,problems,limiter,review,review].forEach((valid,index)=>{const tab=$('#steps li:nth-child('+(index+1)+')');if(tab)tab.dataset.state=valid?'valid':'invalid';});
 }
 function visibility(){
  const mode=rootValue('mode');document.querySelectorAll('[data-mode]').forEach(n=>n.hidden=n.dataset.mode!==mode);const problemGlobs=$('#problem-glob-selection');if(problemGlobs)problemGlobs.hidden=mode==='jobpairs';else $('#problem-selection').hidden=mode==='jobpairs';
  $('#problems-from-jobpairs').hidden=mode!=='jobpairs';updateProblemStatus();
  const imported=['jobpairs','configurations'].includes(mode);$('#building-create').hidden=!imported;$('#imported-build-help').hidden=!imported;
  document.querySelectorAll('.resource').forEach(card=>{const choice=$('.resource-mode',card).value;if(card.dataset.resourceRole==='limiter'){$('[data-limiter-build-toggle]',card).checked=choice!=='none';$('.resource-mode',card).closest('label').hidden=choice==='none';}card.querySelectorAll('[data-recipe]').forEach(n=>n.hidden=n.dataset.recipe!==choice);$('[data-resource-build]',card).hidden=choice==='none';});
  document.querySelectorAll('.configuration').forEach(card=>{const choice=$('.solver-build-mode',card).value;$('[data-solver-build]',card).hidden=choice==='none';updateSolverStatus(card);});updateLimiterStatus();updateStepStates();
 }
 form.addEventListener('change',visibility);visibility();
 function updateProblemStatus(){
  const ready=input=>input.value.trim()&&input.dataset.validated===input.value;
  const mode=rootValue('mode');const problems=[...document.querySelectorAll('[name="glob"]')];
  const inputsReady=mode==='jobpairs'||(problems.length>0&&problems.every(ready));
  const axioms=[...document.querySelectorAll('[name="axiom_glob"]')];
  const axiomsReady=axioms.length>0&&axioms.every(ready);
  for(const [id,valid,label] of [
   ['problem-selection',inputsReady,mode==='jobpairs'?'From jobpairs.csv':inputsReady?`${problems.length} source${problems.length===1?'':'s'} ready`:'Check problem sources'],
   ['axiom-selection',axiomsReady,axiomsReady?`${axioms.length} axiom source${axioms.length===1?'':'s'} ready`:'Check axiom sources']]){
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
 const threadChecks=new WeakMap();
 function checkThreads(input){
  const prior=threadChecks.get(input);if(prior){clearTimeout(prior.timer);prior.controller?.abort();prior.finish();}
  const state={};state.ready=new Promise(resolve=>{state.finish=resolve;});threadChecks.set(input,state);delete input.dataset.threadValid;
  const status=input.closest('label').querySelector('.thread-validation');
  const card=input.closest('.configuration');
  const value=input.value.trim(), selectedHost=rootValue('host'), partition=rootValue('partition');
  const report=(message,valid)=>{status.replaceChildren(document.createTextNode(message));status.className='thread-validation validation '+(valid?'valid':'invalid');input.setAttribute('aria-invalid',String(!valid));};
  if(!/^[1-9][0-9]*$/.test(value)){report('Enter a positive whole number.',false);updateSolverStatus(card);state.finish();return;}
  status.className='thread-validation validation';status.replaceChildren(el('span',undefined,'spinner'),document.createTextNode(' Checking compute nodes…'));input.removeAttribute('aria-invalid');
  updateSolverStatus(card);
  state.timer=setTimeout(async()=>{
   state.controller=new AbortController();
   try{
    const response=await fetch('/api/validate-threads',{method:'POST',signal:state.controller.signal,headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:JSON.stringify({host:selectedHost,partition,threads_per_core:Number(value)})});
    const result=await response.json();if(threadChecks.get(input)!==state||!input.isConnected)return;
    if(!response.ok)throw new Error(result.error||'Could not validate compute nodes.');
    input.dataset.threadValid='true';report(`Valid · ${selectedHost}/${partition} supports up to ${result.supported} threads per core.`,true);
   }catch(error){if(error.name==='AbortError'||threadChecks.get(input)!==state||!input.isConnected)return;report(error.message,false);}
   finally{state.finish();}
   updateSolverStatus(card);
  },350);
 }
 form.addEventListener('input',event=>{if(event.target.name==='threads_per_core')checkThreads(event.target);else if(['host','partition'].includes(event.target.name))form.querySelectorAll('[name="threads_per_core"]').forEach(checkThreads);});
 form.addEventListener('change',event=>{if(['host','partition'].includes(event.target.name))form.querySelectorAll('[name="threads_per_core"]').forEach(checkThreads);});
 form.querySelectorAll('[name="threads_per_core"]').forEach(checkThreads);
 function setCoreRequest(field,value){
  field.dataset.coreRequest=String(value);
  if(![...field.options].some(option=>option.value===String(value)))field.append(el('option',String(value)));
  field.value=String(value);
 }
 function ensureCoreTopology(){
  const selectedHost=rootValue('host'),partition=rootValue('partition'),key=JSON.stringify([selectedHost,partition]);
  if(coreTopologyState?.key===key)return coreTopologyState;
  if(coreTopologyState){clearTimeout(coreTopologyState.timer);coreTopologyState.controller?.abort();coreTopologyState.finish();}
  const state={key,loading:true};state.ready=new Promise(resolve=>{state.finish=resolve;});coreTopologyState=state;
  state.timer=setTimeout(async()=>{
   state.controller=new AbortController();
   try{
    const response=await fetch('/api/partition-core-topology',{method:'POST',signal:state.controller.signal,headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:JSON.stringify({host:selectedHost,partition})});
    const result=await response.json();if(coreTopologyState!==state)return;
    if(!response.ok)throw new Error(result.error||'Could not inspect compute-node topology.');
    if(!Number.isSafeInteger(result.sockets)||!Number.isSafeInteger(result.cores_per_socket)||result.sockets<1||result.cores_per_socket<1)throw new Error('Incomplete compute-node topology.');
    state.topology=result;
   }catch(error){if(error.name==='AbortError'||coreTopologyState!==state)return;state.error=error.message;}
   finally{state.finish();}
   state.loading=false;
   form.querySelectorAll('.configuration').forEach(updateSolverStatus);
  },350);
  return state;
 }
 function syncPlacement(card){
  const cores=card.querySelector('[name="cores"]'),status=card.querySelector('.core-validation');
  const state=ensureCoreTopology(),retry=card.querySelector('[data-core-retry]');
  retry.hidden=!state.error;
  if(!state.topology){
   cores.disabled=true;cores.dataset.coreValid='false';
   status.className='core-validation validation'+(state.error?' invalid':'');
   status.textContent=state.error||'Checking compute-node topology…';return;
  }
  const {sockets,cores_per_socket:perSocket}=state.topology;
  const wholeNode=card.querySelector('[name="exclusive_node"]').value==='true';
  const maximum=!wholeNode&&card.querySelector('[name="exclusive_cpu"]').value==='true'?perSocket:perSocket*sockets;
  if(cores.dataset.placementDefault){setCoreRequest(cores,cores.dataset.placementDefault==='node'?perSocket*sockets:perSocket);delete cores.dataset.placementDefault;}
  let requested=cores.dataset.coreRequest||cores.value||'1';
  if(requested==='all'){requested=String(maximum);cores.dataset.coreRequest=requested;}
  const valid=/^[1-9][0-9]*$/.test(requested)&&Number(requested)<=maximum;
  const signature=JSON.stringify([maximum,requested]);
  if(cores.dataset.coreRange!==signature){
   const options=[];
   if(!valid){const placeholder=el('option','Choose a core count…');placeholder.value='';placeholder.disabled=true;options.push(placeholder);}
   for(let count=1;count<=maximum;count++){const option=el('option',String(count));option.value=String(count);options.push(option);}
   cores.replaceChildren(...options);cores.value=valid?requested:'';cores.dataset.coreRange=signature;
  }
  cores.disabled=false;cores.dataset.coreValid=String(valid);cores.setAttribute('aria-invalid',String(!valid));
  status.className='core-validation validation '+(valid?'valid':'invalid');
  status.textContent=(valid?'':`Previous request (${requested}) exceeds the available range. Choose 1–${maximum}. `)+`${perSocket} cores per socket · ${sockets} sockets per node · maximum ${maximum} cores.`;
 }
 form.addEventListener('change',event=>{
  const card=event.target.closest('.configuration');if(!card)return;
  if(event.target.name==='exclusive_node'){card.querySelector('[name="exclusive_cpu"]').value='false';if(event.target.value==='true')card.querySelector('[name="cores"]').dataset.placementDefault='node';}
  if(event.target.name==='exclusive_cpu'&&event.target.value==='true'&&card.querySelector('[name="exclusive_node"]').value!=='true')card.querySelector('[name="cores"]').dataset.placementDefault='socket';
  if(event.target.name==='cores')event.target.dataset.coreRequest=event.target.value;
  updateSolverStatus(card);
 });
 form.addEventListener('click',event=>{
  if(!event.target.closest('[data-core-retry]'))return;
  coreTopologyState=null;form.querySelectorAll('.configuration').forEach(updateSolverStatus);
 });
 function updateSolverStatus(card){
  syncPlacement(card);
  const value=name=>card.querySelector(`[name="${name}"]`).value.trim();
  card.querySelectorAll('[data-zero-limit]').forEach(option=>{const input=card.querySelector(`[name="${option.dataset.zeroLimit}"]`);option.hidden=!/^0(?:\s*(?:MB|MiB|GB|GiB|TB|TiB))?$/i.test(input.value.trim());});
  const buildSelected=rootValue('building_mode')==='create'&&value('build_mode')==='script';
  const pathReady=name=>{const input=card.querySelector(`[name="${name}"]`);return input.value.trim()&&input.dataset.validated===input.value;};
  const root=value('solver_directory');
  const environment=card.querySelector('[name="environment"]');
  const environmentReady=!environment?.value.trim()||environment.dataset.environmentValidated===environment.value;
  const runtimeReady=environmentReady&&root.startsWith('/')&&!root.includes('/path/to/')&&pathReady('solver_directory')&&
   value('command')&&value('command')!==card.querySelector('[name="command"]').placeholder&&
   card.querySelector('[name="command"]').dataset.commandValidated===solverCommandKey(card);
  const postMode=value('postprocessor_mode');
  card.querySelector('[data-postprocessor-custom]').hidden=postMode!=='custom';
  card.querySelector('[data-postprocessor-default]').hidden=postMode!=='default';
  card.querySelector('[data-postprocessor-active]').hidden=postMode==='none';
  const override=card.querySelector('[name="postprocessor_override"]').checked;
  card.querySelector('[data-postprocessor-override]').hidden=!override;
  const postReady=!override||postMode==='none'||(positiveTimeout(card)&& (postMode==='default'||(pathReady('postprocessor_directory')&&card.querySelector('[name="postprocessor_command"]').dataset.commandValidated===postprocessorKey(card))));
  const buildReady=!buildSelected||((!value('build_source')||pathReady('build_source'))&&pathReady('build_script'));
  const positive=name=>{const input=card.querySelector(`[name="${name}"]`);return input.value.trim()!==''&&input.validity.valid&&Number(input.value)>0;};
  const limitsReady=card.querySelector('[name="threads_per_core"]').dataset.threadValid==='true'&&['wc_limit','cpu_limit'].every(name=>/^(0|[1-9][0-9]*)$/.test(value(name)))&&positive('threads_per_core')&&positive('cores')&&card.querySelector('[name="cores"]').dataset.coreValid==='true'&&
   /^\s*(?:\d+(?:\.\d+)?)(?:\s*(?:[kmgpt]i?b?|b))?\s*$/i.test(value('mem_limit'))&&
   ['exclusive_cpu','exclusive_node'].every(name=>['true','false'].includes(value(name)));
  for(const [section,ready,good,bad] of [
   ['postprocessing',postReady,!override?'Uses workflow setting':postMode==='none'?'Disabled':postMode==='default'?'Built-in SZS parser':'Custom parser ready','Set folder, command and timeout'],
   ['runtime',runtimeReady,'Runtime ready','Set root and command'],
   ['build',buildReady,buildSelected?'Build ready':'No build selected','Set a valid source (or leave blank) and build script'],
   ['limits',limitsReady,'Limits ready','Set limits and placement']]){
   const details=card.querySelector(`[data-solver-section="${section}"]`);details.dataset.state=ready?'valid':'invalid';$('.solver-section-status',details).textContent=ready?good:bad;
  }
  card.dataset.state=Boolean(value('solver_name'))&&[...card.querySelectorAll('.solver-card-section')].every(section=>section.dataset.state==='valid')?'valid':'invalid';
  updateStepStates();
 }
 form.addEventListener('input',event=>{const card=event.target.closest('.configuration');if(card){if(event.target.name==='solver_name')configurationTitle(card);updateSolverStatus(card);}});
 form.addEventListener('change',event=>{const card=event.target.closest('.configuration');if(card)updateSolverStatus(card);});
 const environmentTimers=new WeakMap();
 async function validateEnvironment(card){
  clearTimeout(environmentTimers.get(card));
  const input=card.querySelector('[name="environment"]'),status=card.querySelector('.environment-validation');
  if(!input)return true;
  const original=input.value;
  if(!original.trim()){input.setCustomValidity('');input.dataset.environmentValidated=original;status.textContent='';updateSolverStatus(card);return true;}
  status.className='environment-validation validation';status.textContent='Checking environment variables…';
  try{
   const result=await api('/api/validate-solver-environment',{environment:original});if(input.value!==original)return false;
   input.setCustomValidity('');input.dataset.environmentValidated=original;status.className='environment-validation validation valid';status.textContent=result.message;updateSolverStatus(card);return true;
  }catch(error){if(input.value!==original)return false;input.setCustomValidity(error.message);delete input.dataset.environmentValidated;status.className='environment-validation validation invalid';status.textContent=error.message;updateSolverStatus(card);return false;}
 }
 form.addEventListener('input',event=>{if(event.target.name!=='environment')return;const card=event.target.closest('.configuration');event.target.setCustomValidity('');delete event.target.dataset.environmentValidated;clearTimeout(environmentTimers.get(card));environmentTimers.set(card,setTimeout(()=>validateEnvironment(card),300));});
 form.addEventListener('focusout',event=>{if(event.target.name==='environment')validateEnvironment(event.target.closest('.configuration'));});
 for(const card of document.querySelectorAll('.configuration'))queueMicrotask(()=>validateEnvironment(card));
 function postField(card,name){return card.querySelector(`[name="${card.id==='workflow-postprocessing'?'workflow_':''}postprocessor_${name}"]`);}
 function positiveTimeout(card){const input=postField(card,'timeout');return /^[1-9][0-9]*$/.test(input.value)&&Number(input.value)<=600;}
 function postprocessorData(card){return {directory:postField(card,'directory').value,command:postField(card,'command').value};}
 function postprocessorKey(card){return JSON.stringify(postprocessorData(card));}
 function syncGlobalPostprocessor(){
  const card=$('#workflow-postprocessing');if(!card)return;
  const mode=postField(card,'mode').value,root=postField(card,'directory'),command=postField(card,'command');
  card.querySelector('[data-postprocessor-custom]').hidden=mode!=='custom';card.querySelector('[data-postprocessor-default]').hidden=mode!=='default';card.querySelector('[data-postprocessor-active]').hidden=mode==='none';
  const ready=mode==='none'||(positiveTimeout(card)&&(mode==='default'||(root.value.trim()&&root.dataset.validated===root.value&&command.dataset.commandValidated===postprocessorKey(card))));
  card.dataset.state=ready?'valid':'invalid';card.querySelector('.solver-section-status').textContent=ready?(mode==='none'?'Disabled':mode==='default'?'Built-in SZS parser':'Custom parser ready'):'Set folder, command and timeout';
 }
 function refreshPostprocessing(card){if(card.id==='workflow-postprocessing'){syncGlobalPostprocessor();updateStepStates();}else updateSolverStatus(card);}
 async function validatePostprocessor(card){
  if(card.id!=='workflow-postprocessing'&&!card.querySelector('[name="postprocessor_override"]').checked)return true;
  if(postField(card,'mode').value!=='custom')return true;
  const input=postField(card,'command'),status=card.querySelector('.postprocessor-validation'),key=postprocessorKey(card),data=postprocessorData(card);
  if(input.dataset.commandValidated===key)return true;
  if(!data.directory.trim()||!data.command.trim())return false;
  status.className='postprocessor-validation validation';status.textContent='Checking postprocessor command…';
  try{const result=await api('/api/validate-postprocessor',{...data,allowed_roots:(initial.resources||[]).map(item=>item.root).filter(Boolean)});if(key!==postprocessorKey(card))return false;input.dataset.commandValidated=key;status.className='postprocessor-validation validation valid';status.textContent=result.message;refreshPostprocessing(card);return true;}
  catch(error){if(key!==postprocessorKey(card))return false;delete input.dataset.commandValidated;status.className='postprocessor-validation validation invalid';status.textContent=error.message;refreshPostprocessing(card);return false;}
 }
 const postprocessorTimers=new WeakMap();
 function queuePostprocessor(event){if(!/^(workflow_)?postprocessor_/.test(event.target.name))return;const card=event.target.closest('.configuration,#workflow-postprocessing');if(!card)return;delete postField(card,'command').dataset.commandValidated;clearTimeout(postprocessorTimers.get(card));postprocessorTimers.set(card,setTimeout(()=>validatePostprocessor(card),350));}
 form.addEventListener('input',queuePostprocessor);form.addEventListener('change',queuePostprocessor);
 form.addEventListener('focusout',event=>{if(/^(workflow_)?postprocessor_(directory|command)$/.test(event.target.name))validatePostprocessor(event.target.closest('.configuration,#workflow-postprocessing'));});
 for(const card of document.querySelectorAll('.configuration,#workflow-postprocessing'))queueMicrotask(()=>validatePostprocessor(card));
 $('#workflow-postprocessing').querySelectorAll('textarea').forEach(shellEditor);
 for(const input of $('#workflow-postprocessing').querySelectorAll('[data-path]'))if(input.value.trim())validatePath(input);
 const solverCommandTimers=new WeakMap();
 function solverCommandData(card){return {command:card.querySelector('[name="command"]').value,directory:card.querySelector('[name="solver_directory"]').value,build_on_cluster:card.querySelector('[name="build_mode"]').value==='script'};}
 function solverCommandKey(card){return JSON.stringify(solverCommandData(card));}
 async function validateSolverCommand(card){
  clearTimeout(solverCommandTimers.get(card));
  const input=card.querySelector('[name="command"]'),status=card.querySelector('.command-validation'),key=solverCommandKey(card),data=solverCommandData(card);
  if(!data.command.trim()||!data.directory.trim())return false;
  if(input.dataset.commandValidated===key)return true;
  status.className='command-validation validation';status.textContent='Checking solver executable…';
  try{
   const result=await api('/api/validate-solver-command',data);
   if(key!==solverCommandKey(card))return false;
   input.dataset.commandValidated=key;status.className='command-validation validation valid';status.textContent=result.message;updateSolverStatus(card);return true;
  }catch(error){
   if(key!==solverCommandKey(card))return false;
   delete input.dataset.commandValidated;status.className='command-validation validation invalid';status.textContent=error.message;updateSolverStatus(card);return false;
  }
 }
 function queueSolverCommand(event){
  if(!['command','solver_directory','build_mode'].includes(event.target.name))return;
  const card=event.target.closest('.configuration');if(!card)return;
  delete card.querySelector('[name="command"]').dataset.commandValidated;
  const status=card.querySelector('.command-validation');status.textContent='';
  clearTimeout(solverCommandTimers.get(card));solverCommandTimers.set(card,setTimeout(()=>validateSolverCommand(card),350));
 }
 form.addEventListener('input',queueSolverCommand);form.addEventListener('change',queueSolverCommand);
 form.addEventListener('focusout',event=>{if(['command','solver_directory'].includes(event.target.name)){const card=event.target.closest('.configuration');if(card)validateSolverCommand(card);}});
 for(const card of document.querySelectorAll('.configuration'))queueMicrotask(()=>validateSolverCommand(card));
 async function validatePath(input){
  const status=$('.validation',input.closest('label'));if(!input.value.trim()){if(status)status.textContent='';const card=input.closest('.configuration');if(card)updateSolverStatus(card);return false;}
  const selection=()=>input.name==='glob'?input.closest('.glob-row').querySelector('[name="problem_selection"]')?.value||'':'';
  const originalSelection=selection();
  const original=input.value;if(status){status.className='validation';status.textContent='Checking…';}
  const finish=beginWork(input.name==='glob'?'Checking problem path or archive…':input.name==='axiom_glob'?'Checking axiom path or archive…':'Checking local path…');
  try{const result=await api('/api/validate-path?'+params({host:rootValue('host')}),{kind:input.dataset.path,role:input.name==='axiom_glob'?'axiom':'problem',value:original,selection:originalSelection});if(input.value!==original||selection()!==originalSelection)return false;if(status){status.className='validation valid';status.textContent=result.count?`${result.count} files matched · ${result.path}`:`Found · ${result.path}`;}input.dataset.validated=original;const card=input.closest('.configuration');if(card)updateSolverStatus(card);if(input.name==='glob'||input.name==='axiom_glob')updateProblemStatus();if(input.closest('#limiter-resources'))updateLimiterStatus();updateStepStates();return true;}
  catch(error){if(input.value===original&&selection()===originalSelection&&status){status.className='validation invalid';status.textContent=error.message;}delete input.dataset.validated;const card=input.closest('.configuration');if(card)updateSolverStatus(card);if(input.name==='glob'||input.name==='axiom_glob')updateProblemStatus();if(input.closest('#limiter-resources'))updateLimiterStatus();updateStepStates();return false;}
  finally{finish();}
 }
 form.addEventListener('focusout',event=>{if(event.target.matches('[data-path]'))validatePath(event.target);});
 form.addEventListener('input',event=>{if(event.target.name==='problem_selection'){const input=event.target.closest('.glob-row').querySelector('[name="glob"]');delete input.dataset.validated;}});
 form.addEventListener('focusout',event=>{if(event.target.name==='problem_selection')validatePath(event.target.closest('.glob-row').querySelector('[name="glob"]'));});
 form.addEventListener('change',async event=>{
  if(!event.target.matches('.problem-names-mode,.problem-name-files'))return;
  const row=event.target.closest('.glob-row'),mode=row.querySelector('.problem-names-mode'),picker=row.querySelector('.problem-name-files'),text=row.querySelector('[name="problem_selection"]'),status=row.querySelector('.problem-names-status');
  row.querySelector('.problem-names-files').hidden=mode.value!=='files';
  row.querySelector('.problem-names-text').hidden=mode.value!=='names';
  if(event.target===mode){
   picker._request=null;picker._reading=false;
   if(row._namesMode==='files')row._filesDraft=text.value;else if(row._namesMode==='names')row._namesDraft=text.value;
   text.value=mode.value==='files'?(row._filesDraft||''):mode.value==='names'?(row._namesDraft||''):'';
   row._namesMode=mode.value;picker.setCustomValidity(mode.value==='files'&&!text.value.trim()&&!picker.files.length?'Choose one or more files of problem names.':'');
   status.textContent=mode.value==='files'&&text.value.trim()?row._nameFiles.length?`${row._nameFiles.length} name ${row._nameFiles.length===1?'file':'files'} selected.`:'No name files added yet.':'';
   text.dispatchEvent(new Event('input',{bubbles:true}));await validatePath(row.querySelector('[name="glob"]'));return;
  }
  const files=[...picker.files];if(!files.length)return;
  const request={};picker._request=request;picker.setCustomValidity('Reading files of problem names…');status.textContent='Reading files…';picker._reading=true;
  const finish=beginWork('Reading files of problem names…');
  try{
   const contents=await Promise.all(files.map(file=>file.text()));if(picker._request!==request)return;
   row._nameFiles=(row._nameFiles||[]).concat(files.map((file,index)=>({name:file.name,content:contents[index].replace(/^\uFEFF/,'')})));
   text.value=nameFileContents(row._nameFiles);row._filesDraft=text.value;renderNameFiles(row);picker.value='';
   picker.setCustomValidity('');status.textContent=`${row._nameFiles.length} name ${row._nameFiles.length===1?'file':'files'} selected.`;
   text.dispatchEvent(new Event('input',{bubbles:true}));await validatePath(row.querySelector('[name="glob"]'));
  }catch(error){if(picker._request===request){picker.setCustomValidity('Could not read the selected files. Choose them again.');status.textContent='Could not read the selected files. The previous names have been kept.';}}
  finally{if(picker._request===request)picker._reading=false;finish();}
 });

 for(const card of document.querySelectorAll('.configuration'))for(const input of card.querySelectorAll('[data-path]')){
  if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 }
 for(const input of document.querySelectorAll('[name="glob"],[name="axiom_glob"]'))if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 for(const input of document.querySelectorAll('#limiter-resources [data-path]'))if(input.value.trim()&&!input.value.includes('/path/to/'))validatePath(input);
 function fields(card){return Object.fromEntries([...card.querySelectorAll('[name]')].map(n=>[n.name,n.type==='checkbox'?n.checked:n.value]));}
 function payload(){const names=['name','partition','account','reservation','batch_size','max_outstanding','pair_order','mode','jobpairs','configurations_file','building_mode','building_file','limiter_mode','limiter_file','limiter'];const data=Object.fromEntries(names.map(n=>[n,rootValue(n)]));data.host=rootValue('host');data.postprocessor=Object.fromEntries(['mode','directory','command','timeout'].map(key=>[key,postField($('#workflow-postprocessing'),key).value]));data.get_computer_info=form.querySelector('[name="get_computer_info"]').checked;const cards=[...document.querySelectorAll('.configuration')];data.configurations=cards.map(card=>{const row=fields(card);for(const key of Object.keys(row))if(key.startsWith('build_'))delete row[key];row.cpus='auto';return row;});data.resources=[...document.querySelectorAll('.resource')].sort((a,b)=>Number(a.dataset.resourceOrder)-Number(b.dataset.resourceOrder)).filter(node=>data.mode!=='interactive'||node.dataset.resourceRole==='limiter').map(node=>({...fields(node),role:node.dataset.resourceRole}));if(data.mode==='interactive'&&data.building_mode==='create'){const seen=new Map();for(const card of cards){const value=fields(card);const resource={role:'solver',root:value.solver_directory,mode:value.build_mode,source:value.build_source,script:value.build_script,build_policy:value.build_policy,artifact:value.build_mode==='script'?solverArtifact(value.command,value.solver_directory):''};const previous=seen.get(resource.root);if(previous){if(resource.mode!=='none'&&previous.mode!=='none'&&JSON.stringify(resource)!==JSON.stringify(previous))throw new Error(`Conflicting build settings for solver root ${resource.root}`);if(previous.mode==='none'&&resource.mode!=='none')Object.assign(previous,resource);}else{seen.set(resource.root,resource);data.resources.push(resource);}}}data.globs=[...document.querySelectorAll('[name="glob"]')].map(n=>n.value);data.problem_selections=[...document.querySelectorAll('[name="problem_selection"]')].map(n=>n.value);data.problem_selection_files=[...document.querySelectorAll('#globs .glob-row')].map(row=>row._nameFiles||[]);data.problem_selection_modes=[...document.querySelectorAll('.problem-names-mode')].map(n=>n.value);data.axiom_mode='custom';data.axiom_globs=[...document.querySelectorAll('[name="axiom_glob"]')].map(n=>n.value);return data;}
 function solverArtifact(command,root){const executable=command.trim().match(/^(?:'([^']+)'|"([^"]+)"|(\S+))/)?.slice(1).find(Boolean)||'';if(!executable)throw new Error('Enter the solver command before configuring its build.');const path=executable.startsWith(root+'/')?executable.slice(root.length+1):executable.replace(/^\.\//,'');if(path.startsWith('/')||path==='..'||path.startsWith('../')||path.includes('/../')||path.includes('{{'))throw new Error('For a remote build, start the solver command with an executable relative to the solver root.');return path;}
 const batchInput=form.querySelector('[name="batch_size"]');
 let batchRequest=0,batchTimer=null,lastBatchSuggestion=null;
 let acceptedBatchWarning=null,batchFitCheck=null;
 const batchDialog=$('#batch-explanation-dialog');let lastBatchPlan=null;
 $('#close-batch-explanation').addEventListener('click',()=>batchDialog.close());
 $('#explain-batch-suggestion').addEventListener('click',async()=>{batchDialog.showModal();if(!lastBatchPlan)await suggestBatchSize();if(lastBatchPlan)renderBatchExplanation(lastBatchPlan);else text('#batch-explanation-content','The calculation is unavailable. Complete the solver and problem settings, then try again.');});
 function renderBatchExplanation(data){
  const content=$('#batch-explanation-content');content.replaceChildren();
  const flow=el('div',undefined,'batch-math-flow');content.append(flow);
  const amount=(number,singular,plural=singular+'s')=>`${number.toLocaleString()} ${number===1?singular:plural}`;
  function calculation(value){
   const code=el('code',undefined,'batch-calculation');let offset=0;
   for(const match of value.matchAll(/\b(?:floor|ceil|min|max)(?=\s*\()|\d[\d,]*(?:\.\d+)?|[÷×=+−<>()]/g)){
    if(match.index>offset)code.append(el('span',value.slice(offset,match.index),'math-unit'));
    code.append(el('span',match[0],/^(floor|ceil|min|max)$/.test(match[0])?'math-function':/^\d/.test(match[0])?'math-number':'math-operator'));
    offset=match.index+match[0].length;
   }
   if(offset<value.length)code.append(el('span',value.slice(offset),'math-unit'));
   return code;
  }
  function card(title,formula,explanation){const node=el('article',undefined,'batch-math-card');node.append(el('h3',title),calculation(formula),el('p',explanation));flow.append(node);return node;}
  function explain(node,formula,meaning){node.append(calculation(formula),el('p',meaning));}
  const pairs=amount(data.total_pairs,'solver-problem pair');
  const accountLimit=data.account_submit_limit===undefined?50:data.account_submit_limit;
  const accountSlots=accountLimit===null?'':`, ${amount(accountLimit,'account task slot')}`;
  card('1. Count the Work',data.problem_count===null?pairs:`${amount(data.configuration_count,'solver configuration')} × ${amount(data.problem_count,'distinct problem')} = ${pairs}`,data.problem_count===null?'Each explicit solver-problem pair is one row in the input file.':'Every solver configuration is paired with every selected problem. Overlapping problem sources count only once.');
  const hardware=card('2. Find Parallel Capacity',amount(data.hardware_parallelism,'parallel batch','parallel batches')+' at full capacity',`${amount(data.node_count,'compute node')} in the selected partition. For each solver, calculate how many batches its cores and placement allow. Use the smallest capacity across solvers. This counts total capacity, not currently idle nodes.`);
  data.configuration_parallelism.forEach((count,index)=>{
   hardware.append(el('p',data.configuration_names[index],'batch-solver-name'));
   const groups=data.capacity_breakdown?.[index];
   const formula=groups?.map(group=>`${amount(group.nodes,'compute node')} × ${group.formula}`).join(' + ')||'Hardware capacity';
   hardware.append(calculation(`${formula} = ${amount(count,'parallel batch','parallel batches')}`));
  });
  const halfFormula=data.hardware_parallelism<2?`max(1 parallel batch, floor(${amount(data.hardware_parallelism,'parallel batch','parallel batches')} ÷ 2))`:`floor(${amount(data.hardware_parallelism,'parallel batch','parallel batches')} ÷ 2)`;
  const conservative=card('3. Assume Half the Capacity Is Available',`${halfFormula} = ${amount(data.assumed_hardware_parallelism,'assumed parallel batch','assumed parallel batches')}`,'Divide the hardware capacity by 2 to leave room for other cluster users. floor rounds DOWN to a whole batch; always allow at least one. This is a conservative assumption, not a measurement of current availability.');
  explain(conservative,`min(${amount(data.assumed_hardware_parallelism,'assumed parallel batch','assumed parallel batches')}, ${amount(data.workflow_submit_limit,'workflow task slot')}${accountSlots}) = ${amount(data.assumed_parallelism,'parallel batch','parallel batches')}`,'A batch uses one SLURM task. min chooses the smallest allowance: the assumed hardware capacity, your workflow submission limit, and any reported account limit.');
  const quotient=data.total_pairs/data.assumed_parallelism;
  const fraction=quotient.toLocaleString(undefined,{maximumSignificantDigits:4});
  explain(conservative,`ceil(${pairs} ÷ ${amount(data.assumed_parallelism,'parallel batch','parallel batches')}) = ${amount(data.balance_batch_size,'pair','pairs')}/batch`,`Dividing the pairs among the assumed parallel batches gives ${fraction} pairs per batch. ceil rounds UP so the batch size is a whole number: ${amount(data.balance_batch_size,'solver-problem pair')} per batch.${data.total_pairs<data.assumed_parallelism?' There are fewer pairs than parallel slots, so some slots are unused; no empty batches are created.':''}`);
  const fit=card('4. Fit the Submission Window',data.fit_batch_size?`At least ${amount(data.fit_batch_size,'solver-problem pair')} per batch`:'No batch size fits immediately','Any reported account limit includes your running and pending SLURM tasks in that account, including tasks outside Slurmy.');
  if(accountLimit===null){fit.append(el('p',`No account submission cap was reported for ${data.account||'the selected account'}. The workflow submission window applies; SLURM still enforces any account or QOS policy.`));}
  else if(data.account_active_tasks!==null){
   explain(fit,`max(0 task slots, ${amount(accountLimit,'account task slot')} − ${amount(data.account_active_tasks,'submitted task')}) = ${amount(data.account_free_slots,'free task slot')}`,'Subtract tasks already submitted by your cluster user. Do not count a negative number of free slots.');
  }else fit.append(el('p',`Current account usage is unknown; this estimate assumes ${accountLimit} free task slots.`));
  if(accountLimit!==null)explain(fit,`min(${amount(data.account_free_slots,'free account task slot')}, ${amount(data.workflow_submit_limit,'workflow task slot')}) = ${amount(data.submission_budget,'usable task slot')}`,'This is how many batches can be submitted together under both limits. Availability can change.');
  fit.append(el('p',`${amount(data.minimum_batches,'separate group')} must be kept separate because their SLURM allocation settings differ. Round each group’s batch count UP, then add the counts. Find the smallest batch size whose total fits the usable task slots.`));
  function groupCount(node,size,total){
   const counts=data.group_pair_counts||[data.total_pairs];
   const terms=counts.slice(0,6).map(count=>`ceil(${amount(count,'pair')} ÷ ${amount(size,'pair','pairs')}/batch)`);
   if(counts.length>6)terms.push('…');
   explain(node,`${terms.join(' + ')} = ${amount(total,'SLURM batch','SLURM batches')}`,counts.length>6?`The sum includes all ${amount(counts.length,'group')}; only the first six terms are shown.`:'Each rounded-up term is the number of batches for one group.');
  }
  if(data.fit_batch_size){
   const counts=data.group_pair_counts||[data.total_pairs];
   groupCount(fit,data.fit_batch_size,counts.reduce((sum,count)=>sum+Math.ceil(count/data.fit_batch_size),0));
  }
  if(data.wall_limit_seconds){
   const time=card('5. Respect the SLURM Wall-Time Limit',`${amount(data.wall_limit_seconds,'second')}/batch = ${data.wall_limit_label||'reported SLURM limit'}/batch`,'Use the largest wall-clock limit of any pair as a conservative bound. Include the same startup and per-pair overhead as the submission planner.');
   if(data.wall_time_unknown)time.append(el('p','A zero wall limit is unbounded, so no finite batch size can be guaranteed to finish within the reported SLURM time limit. The capacity/window estimate is retained; SLURM may stop an unfinished batch at its allocation limit.'));
   else{
    (data.configuration_wall_seconds||[]).forEach((wall,index)=>explain(time,`${data.configuration_names[index]}: ${amount(wall,'second')}/pair wall + ${amount(data.pair_overhead_seconds,'second')}/pair overhead + ${amount(data.configuration_postprocessor_seconds[index],'second')}/pair postprocessing = ${amount(wall+data.pair_overhead_seconds+data.configuration_postprocessor_seconds[index],'second')}/pair`,'Active postprocessing reserves timeout + 5 seconds; disabled postprocessing reserves zero.'));
    explain(time,`floor((${amount(data.wall_limit_seconds,'second')}/batch − ${amount(data.batch_startup_seconds,'second')}/batch startup) ÷ ${amount(data.max_pair_budget_seconds,'second')}/pair combined budget) = ${amount(data.wall_time_capacity,'pair')}/batch`,'Postprocessing reserves timeout + 5 seconds per active pair; None adds zero. The capacity uses the largest combined pair budget. Faster pairs may finish sooner.');
    if(!data.wall_single_pair_fits)time.append(el('p','Even one pair can exceed this budget. A batch size of 1 is suggested, but finishing before the SLURM limit cannot be guaranteed.'));
   }
  }
  const chosen=data.wall_time_capacity!=null?`min(${amount(data.unconstrained_batch_size,'pair','pairs')}/batch, ${amount(Math.max(1,data.wall_time_capacity),'pair','pairs')}/batch) = ${amount(data.batch_size,'pair','pairs')}/batch`:
   data.fit_batch_size?`max(${amount(data.balance_batch_size,'pair','pairs')}/batch, ${amount(data.fit_batch_size,'pair','pairs')}/batch) = ${amount(data.batch_size,'pair','pairs')}/batch`:`Use ${amount(data.batch_size,'pair')} per batch as the half-capacity estimate`;
  const final=card(data.wall_limit_seconds?'6. Choose the Batch Size':'5. Use the Larger Size',chosen,'Start with the larger capacity/window estimate, then cap it by the wall-time budget when a finite bound is available. If the time cap requires more batches than the submission window, later batches must wait locally.');
  groupCount(final,data.batch_size,data.suggested_batches);
  final.append(el('p',data.suggested_fits?`${amount(data.suggested_batches,'batch','batches')} fit the ${amount(data.submission_budget,'usable task slot')}.`:'Some batches must wait. Keep the local server running until all batches are accepted.'));
  final.append(el('p','Each batch runs its pairs sequentially; different batches can run in parallel.'));
  const picture=el('div',undefined,'batch-window-picture');picture.setAttribute('aria-label',`${data.suggested_batches} suggested batches; ${data.submission_budget} available submission slots`);
  const shown=Math.min(data.suggested_batches,100);for(let i=0;i<shown;i++){const square=el('span',String(i+1),i<data.submission_budget?'batch-slot fits':'batch-slot waits');square.title=`Batch ${i+1}: ${i<data.submission_budget?'fits the current submission window':'must wait for a free submission slot'}`;picture.append(square);}content.append(picture,el('p','Each numbered box is one SLURM batch.'+(data.suggested_batches>100?' Only the first 100 are shown.':'')+' Account usage can change after this calculation.','muted'));
 }
 function updateBatchTimeWarning(){
  const warning=$('#batch-time-warning'),data=lastBatchPlan,size=Number(batchInput.value);
  if(!data||!data.wall_limit_seconds||!Number.isInteger(size)||size<1){warning.hidden=true;return;}
  const budget=data.wall_time_unknown?604800:data.batch_startup_seconds+size*data.max_pair_budget_seconds;
  warning.hidden=budget<=data.wall_limit_seconds;
  warning.textContent=warning.hidden?'':`This batch size may need more than the ${data.wall_limit_label||'reported'} SLURM limit for ${data.account||'the selected account'}. Slurmy will cap the requested time at that limit. SLURM may stop unfinished work; completed solver-problem pairs are kept and the rest can be resumed.`;
 }
 async function suggestBatchSize(){
  const request=++batchRequest, status=$('#batch-suggestion'), use=$('#use-batch-suggestion');
  status.className='validation';status.textContent='Checking partition nodes and counting solver-problem pairs…';use.hidden=true;
  try{
   const data=await api('/api/suggest-batch-size',payload());
   if(request!==batchRequest)return;
   lastBatchSuggestion=data.batch_size;lastBatchPlan=data;renderBatchExplanation(data);updateBatchTimeWarning();
   status.className='validation valid';
   status.textContent=`Suggested: ${data.batch_size} ${data.batch_size===1?'pair':'pairs'} per batch · ${data.suggested_batches} ${data.suggested_batches===1?'batch':'batches'}. ${data.suggested_fits?'Fits the current submission window.':'Some batches will need to wait.'}${data.wall_time_policy?.policy_verified===false?' Account/QOS policy could not be fully verified; the reported limits are used conservatively.':''}`;
   use.hidden=Number(batchInput.value)===data.batch_size;
   updateStepStates();
  }catch(error){if(request!==batchRequest)return;lastBatchSuggestion=null;lastBatchPlan=null;updateBatchTimeWarning();status.className='validation invalid';status.textContent=`Could not suggest a batch size: ${error.message} You can enter a size manually.`;}
 }
 batchInput.addEventListener('input',()=>{updateBatchTimeWarning();$('#use-batch-suggestion').hidden=lastBatchSuggestion===null||Number(batchInput.value)===lastBatchSuggestion;});
 $('#use-batch-suggestion').addEventListener('click',()=>{if(lastBatchSuggestion!==null){batchInput.value=lastBatchSuggestion;$('#use-batch-suggestion').hidden=true;updateBatchTimeWarning();updateStepStates();}});
 form.addEventListener('input',event=>{
  if(event.target===batchInput)return;
  ++batchRequest;lastBatchSuggestion=null;lastBatchPlan=null;updateBatchTimeWarning();clearTimeout(batchTimer);
  if(step===4)batchTimer=setTimeout(suggestBatchSize,400);
 });
 form.addEventListener('change',event=>{if(event.target!==batchInput&&step===4){++batchRequest;clearTimeout(batchTimer);batchTimer=setTimeout(suggestBatchSize,400);}});
 function renderStep(){sections.forEach((s,i)=>s.hidden=i!==step);document.querySelectorAll('#steps li').forEach((n,i)=>{n.classList.toggle('current',i===step);const tab=$('button',n);if(i===step)tab.setAttribute('aria-current','step');else tab.removeAttribute('aria-current');});$('#builder-back').hidden=step===0;$('#builder-next').hidden=step===5;$('#builder-save').hidden=step!==5;text('#step-count',`Step ${step+1} of 6`);$('#builder-error').hidden=true;if(step===4)suggestBatchSize();}
 function showError(error){text('#builder-error',error.message);$('#builder-error').hidden=false;}
 async function waitForPlacement(card){
  const input=card.querySelector('[name="threads_per_core"]');
  while(card.isConnected){
   const topology=ensureCoreTopology(),threads=threadChecks.get(input);
   await Promise.all([topology.ready,threads?.ready]);
   if(topology===coreTopologyState&&threads===threadChecks.get(input))return;
  }
 }
 async function checkSubmissionFit(){
  if(batchFitCheck)return batchFitCheck;
  batchFitCheck=checkSubmissionFitNow();
  try{return await batchFitCheck;}finally{batchFitCheck=null;}
 }
 async function checkSubmissionFitNow(){
  const current=payload(), signature=JSON.stringify(current);
  if(acceptedBatchWarning===signature)return;
  const plan=await api('/api/suggest-batch-size',current);
  if(signature!==JSON.stringify(payload()))return checkSubmissionFitNow();
  if(plan.estimated_batches<=plan.submission_budget&&!plan.selected_size_exceeds_wall)return;
  const recommendation=plan.recommended_batch_size;
  const timeWarning=plan.selected_size_exceeds_wall?`The selected batch size can exceed the ${plan.wall_limit_label||'reported'} SLURM limit for ${plan.account||'this account'}. Slurmy will cap the requested time at that limit. Completed solver-problem pairs are kept; unfinished pairs can be resumed. ${plan.wall_time_unknown?'A zero wall limit cannot guarantee completion within the SLURM allocation. ':`The conservative maximum is ${plan.wall_time_capacity} pairs per batch, including overhead. `}`:'';
  const explanation=timeWarning+`This workflow creates approximately ${plan.estimated_batches} batches. ${plan.account_submit_limit===null?`No account submission cap was reported for ${plan.account||'the selected account'}.`:`The selected account permits up to ${plan.account_submit_limit??50} submitted SLURM tasks for your user, including pending tasks and tasks outside Slurmy.`} Your submitted-task setting is ${current.max_outstanding}.${plan.account_submit_limit!==null&&plan.account_active_tasks!==null?` Your account currently has ${plan.account_active_tasks} active/pending tasks, leaving ${plan.account_free_slots} slots.`:''} Keep this computer awake and the local server running until the final batch is accepted; already submitted tasks continue if you disconnect. `+(recommendation?`Use ${recommendation} solver-problem pairs per batch to create ${plan.recommended_batches} ${plan.recommended_batches===1?'batch':'batches'}, ${plan.suggested_fits?'fitting within the current submission window and any reported account limit':'respecting the time budget, but requiring some batches to wait for submission slots'}. Other submissions can change availability.`:`${plan.submission_budget===0?'Your account currently has no free submission slots; even a single batch must wait. ':''}At least ${plan.minimum_batches} separate batches are required by the distinct SLURM allocation settings. Increasing batch size alone cannot fit them into the current ${plan.submission_budget}-task window. Adjust the allocation requirements or submitted-task setting, or allow local submission to wait.`);
  const choice=await SlurmyDialog.choose({title:plan.selected_size_exceeds_wall?'Batch Size Can Exceed the SLURM Time Limit':'Some Batches Will Need to Wait',message:explanation,choices:[...(recommendation?[{value:'use',label:`Use Batch Size ${recommendation}`}]:[]),{value:'keep',label:'Keep Batch Size',secondary:true}]});
  if(!choice)throw new Error('Review the batch size before continuing.');
  if(choice==='use'){batchInput.value=recommendation;updateBatchTimeWarning();updateStepStates();}
  acceptedBatchWarning=JSON.stringify(payload());
 }
 async function validSection(){
  if(step===2)for(const row of document.querySelectorAll('#globs .glob-row')){
   if(row.querySelector('.problem-names-mode').value!=='files')continue;
   const picker=row.querySelector('.problem-name-files');
   if(picker._reading)throw new Error('Wait for the selected name files to finish loading.');
   if(picker.validationMessage)throw new Error(picker.validationMessage);
   if(!row.querySelector('[name="problem_selection"]').value.trim())throw new Error('Add a non-empty file of problem names, or choose All matched problems.');
  }

  if(step===1&&rootValue('mode')==='interactive'&&!$('#configurations .configuration'))throw new Error('Add a solver configuration or copy one from an existing workflow before continuing.');
  if(step===1&&rootValue('mode')==='interactive')await Promise.all([...document.querySelectorAll('.configuration')].map(waitForPlacement));
  if(step===1&&rootValue('mode')==='interactive')for(const card of document.querySelectorAll('.configuration'))await validateEnvironment(card);
  if(step===1&&rootValue('mode')==='interactive'&&!await validatePostprocessor($('#workflow-postprocessing'))){$('#workflow-postprocessing').open=true;throw new Error('Correct the workflow postprocessor folder and command.');}
  const inputs=[...sections[step].querySelectorAll('input,textarea,select')].filter(n=>n.type!=='hidden'&&!n.closest('[hidden]'));
  for(const input of inputs){if(!input.value.trim()&&!input.dataset.optional){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;input.focus();throw new Error('Fill in each visible setting before continuing.');}if(!input.checkValidity()){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;input.reportValidity();throw new Error('Please correct the highlighted field.');}}
  for(const input of inputs.filter(n=>n.dataset.path&&n.value.trim())){if(input.dataset.validated!==input.value&&!await validatePath(input)){const details=input.closest('.solver-card-section,.problem-card-section,.limiter-card-section');if(details)details.open=true;throw new Error('Correct the path highlighted above.');}}
  if(step===1&&rootValue('mode')==='interactive'){syncGlobalPostprocessor();if($('#workflow-postprocessing').dataset.state!=='valid'){$('#workflow-postprocessing').open=true;throw new Error('Complete workflow postprocessing before continuing.');}}
  if(step===1&&rootValue('mode')==='interactive')for(const card of document.querySelectorAll('.configuration')){
   if(!await validatePostprocessor(card)){const section=card.querySelector('[data-solver-section="postprocessing"]');section.open=true;throw new Error('Correct the custom postprocessor folder and command.');}
   if(!await validateSolverCommand(card))throw new Error('Correct the solver executable highlighted under Command.');
   updateSolverStatus(card);const section=card.querySelector('.solver-card-section[data-state="invalid"]');if(section&&!section.hidden){section.open=true;section.querySelector('input,textarea,select')?.focus();throw new Error('Complete the highlighted solver section before continuing.');}
  }
  if(step===1&&rootValue('mode')==='interactive'&&[...document.querySelectorAll('#configurations .configuration')].some(card=>card.dataset.state==='invalid'))throw new Error('Give each solver configuration a distinct name.');
  if(step===2){updateProblemStatus();const section=sections[step].querySelector('.problem-card-section[data-state="invalid"]');if(section){section.open=true;throw new Error('Complete the highlighted problem or axiom section before continuing.');}}
  if(step===4||step===5)await checkSubmissionFit();
  if(step===3){updateLimiterStatus();const section=sections[step].querySelector('.limiter-card-section[data-state="invalid"]');if(section){section.open=true;throw new Error('Complete the highlighted limiter section before continuing.');}}
 }
 let navigating=false;
 async function navigate(target){if(navigating||target===step)return;navigating=true;const finish=beginWork('Checking this step before continuing…');const tabs=[...document.querySelectorAll('#steps button')];tabs.forEach(tab=>tab.disabled=true);$('#builder-next').disabled=true;
  try{if(target<step){step=target;renderStep();return;}for(let current=step;current<target;current++){step=current;renderStep();await validSection();}step=target;renderStep();}
  catch(error){if(error.cancelled)notice(error.message);else showError(error);}finally{finish();navigating=false;tabs.forEach(tab=>tab.disabled=false);$('#builder-next').disabled=false;}
 }
 document.querySelectorAll('#steps button').forEach((tab,index)=>tab.addEventListener('click',()=>navigate(index)));
 batchInput.addEventListener('change',async()=>{if(step!==4||navigating||!batchInput.checkValidity())return;try{await checkSubmissionFit();}catch(error){showError(error);}});
 $('#builder-back').addEventListener('click',()=>{step--;renderStep();});
 $('#builder-next').addEventListener('click',()=>navigate(step+1));
 $('#builder-save').addEventListener('click',async()=>{const save=$('#builder-save');save.disabled=true;const finish=beginWork('Saving the complete workflow and solver-problem pair list…');try{await validSection();const current=payload();const endpoint=editing?'/api/workflows/update?stream=1&'+params({directory:context.dataset.directory}):'/api/workflows?stream=1';const data=await streamBuilder(endpoint,current,finish.update);location.href='/workflow?'+new URLSearchParams({host:current.host,directory:data.directory});}catch(error){if(error.cancelled)notice(error.message);else showError(error);save.disabled=false;}finally{finish();}});
 form.addEventListener('input',updateStepStates);form.addEventListener('change',updateStepStates);
 form.addEventListener('submit',e=>e.preventDefault());renderStep();updateStepStates();
}
