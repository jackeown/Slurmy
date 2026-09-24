'use strict';
const $ = (selector, root=document) => root.querySelector(selector);
const host = document.body.dataset.host;
const csrf = $('meta[name="csrf-token"]')?.content;
const page = document.body.dataset.page;
const params = values => new URLSearchParams({host, ...values}).toString();
const el = (tag, text, cls) => {const node=document.createElement(tag); if(text!==undefined)node.textContent=text; if(cls)node.className=cls; return node;};
const link = (text, href) => {const node=el('a',text); node.href=href; return node;};
async function api(path, data) {
  const response = await fetch(path, data === undefined ? {} : {method:'POST', headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:JSON.stringify(data)});
  const value = await response.json();
  if (!response.ok) throw new Error(value.error || `Request failed (${response.status})`);
  return value;
}
function notice(text){const node=$('#notice');node.textContent=text;node.hidden=false;setTimeout(()=>node.hidden=true,6500);}
function duration(value){if(value==null)return '—';if(value===0)return '0 s';if(value<1)return `${(value*1000).toFixed(value<.01?1:0)} ms`;if(value<60)return `${value.toFixed(2)} s`;if(value<3600)return `${Math.floor(value/60)}m ${(value%60).toFixed(0)}s`;return `${Math.floor(value/3600)}h ${Math.floor(value%3600/60)}m`;}
function memory(value){if(value==null)return '—';let i=0;const units=['B','KiB','MiB','GiB','TiB'];while(value>=1024&&i<units.length-1){value/=1024;i++;}return `${value.toFixed(i===0?0:1)} ${units[i]}`;}
function badge(state){const value=state.toLowerCase();let cls='';if(/error|fail|incomplete|interrupted|cancel|not run/.test(value))cls='bad';else if(/limit|timeout/.test(value))cls='limit';else if(/run|pending|submitted|completing/.test(value))cls='active';else if(/done|ok|completed/.test(value))cls='good';return el('span',state,`badge ${cls}`);}
function text(id, value){const node=$(id);if(node)node.textContent=value;}
function button(label, action, cls='secondary'){const node=el('button',label,cls);node.type='button';node.addEventListener('click',action);return node;}
function tableRow(cells){const tr=el('tr');for(const cell of cells){const td=el('td');td.append(cell instanceof Node?cell:document.createTextNode(cell??'—'));tr.append(td);}return tr;}
function sortableValue(value){
 const text=value.trim().replace(/[,%]/g,'');
 const number=Number(text);
 return text!==''&&!Number.isNaN(number)?number:text.toLocaleLowerCase();
}
function initTableSorting(){
 document.querySelectorAll('table').forEach(table=>{
  table.querySelectorAll('thead th').forEach((th,index)=>{
   if(th.querySelector('.sort-button'))return;
   const label=th.textContent.trim()||`Column ${index+1}`;
   th.textContent='';
   const control=el('button',`${label} ↕`,'sort-button secondary');
   control.type='button';control.title=`Sort by ${label}`;control.setAttribute('aria-label',`Sort by ${label}`);
   control.dataset.direction='none';
   control.addEventListener('click',()=>{
    const direction=control.dataset.direction==='asc'?'desc':'asc';
    table.querySelectorAll('thead .sort-button').forEach(button=>{button.dataset.direction='none';button.textContent=`${button.dataset.label} ↕`;button.removeAttribute('aria-sort');});
    control.dataset.label=label;control.dataset.direction=direction;control.textContent=`${label} ${direction==='asc'?'↑':'↓'}`;control.setAttribute('aria-sort',direction);
    const body=table.tBodies[0];if(!body)return;
    [...body.rows].sort((a,b)=>{const av=sortableValue(a.cells[index]?.textContent||'');const bv=sortableValue(b.cells[index]?.textContent||'');if(av===bv)return 0;const result=av>bv?1:-1;return direction==='asc'?result:-result;}).forEach(row=>body.append(row));
   });
   control.dataset.label=label;th.append(control);
  });
 });
}
function initCollapsibles(){
 const elements=[...document.querySelectorAll('section, .panel, .form-card')].filter((node,index,list)=>list.indexOf(node)===index&&!node.matches('details')&&!node.closest('template'));
 elements.forEach(section=>{
  if(section.dataset.collapsibleReady)return;
  const header=section.querySelector(':scope > .section-heading, :scope > h2, :scope > h3, :scope > div:first-child');
  if(!header)return;
  const caption=header.matches('h2,h3,strong')?header:header.querySelector('h2, h3, strong');
  if(!caption)return;
  section.dataset.collapsibleReady='true';header.classList.add('collapse-header');caption.classList.add('collapse-caption');
  const control=el('button',undefined,'collapse-toggle');control.type='button';control.setAttribute('aria-expanded','true');control.setAttribute('aria-label',`Collapse ${caption.textContent.trim()}`);control.title='Collapse section';
  const toggle=()=>{const collapsed=section.classList.toggle('collapsed');control.setAttribute('aria-expanded',String(!collapsed));control.setAttribute('aria-label',`${collapsed?'Expand':'Collapse'} ${caption.textContent.trim()}`);control.title=`${collapsed?'Expand':'Collapse'} section`;};
  control.addEventListener('click',event=>{event.stopPropagation();toggle();});
  header.addEventListener('click',event=>{if(event.target.closest('button, a, input, select, textarea, label, summary'))return;toggle();});
  caption.prepend(control);
 });
}
window.initCollapsibles=initCollapsibles;
initTableSorting();
initCollapsibles();
const themeToggle=$('#theme-toggle');
function setTheme(theme){document.documentElement.dataset.theme=theme;themeToggle?.setAttribute('aria-pressed',String(theme==='dark'));const label=themeToggle?.querySelector('span:last-child');if(label)label.textContent=theme==='dark'?'Light mode':'Dark mode';}
setTheme(document.documentElement.dataset.theme||'light');
themeToggle?.addEventListener('click',()=>{const theme=document.documentElement.dataset.theme==='dark'?'light':'dark';localStorage.setItem('slurmy-theme',theme);setTheme(theme);});
$('#host-form')?.addEventListener('submit',event=>{event.preventDefault();const url=new URL(location.href);url.searchParams.set('host',$('#host').value);location.href=url;});

let operationTimer;
function lockWorkflowActions(locked){if(page!=='workflow')return;for(const id of ['prepare','submit']){const control=$('#'+id);if(control)control.disabled=locked;}}
async function watchOperation(id){
  clearTimeout(operationTimer);$('#operation').hidden=false;sessionStorage.setItem('slurmy-operation',id);
  try{
    const op=await api('/api/operations/'+encodeURIComponent(id));text('#operation-title',`${op.label} · ${op.state}`);text('#operation-log',op.log||'Starting…');
    const match=op.log.match(/Slurmy job ID: ([A-Za-z0-9._-]+)/);
    $('#submitted-link')?.remove();
    if(match){announceJob(match[1]);renderJobs();const node=link('View submitted job →','/jobs/'+encodeURIComponent(match[1])+'?'+params({}));node.id='submitted-link';$('#operation').append(node);refresh();}
    if(op.state==='running'){lockWorkflowActions(true);operationTimer=setTimeout(()=>watchOperation(id),1500);}
    else{lockWorkflowActions(false);sessionStorage.removeItem('slurmy-operation');await refresh();}
  }catch(error){lockWorkflowActions(false);text('#operation-log',error.message);sessionStorage.removeItem('slurmy-operation');}
}
$('#dismiss-operation')?.addEventListener('click',()=>{$('#operation').hidden=true;clearTimeout(operationTimer);sessionStorage.removeItem('slurmy-operation');});
async function startOperation(url,data){try{const op=await api(url,data);lockWorkflowActions(true);watchOperation(op.id);}catch(error){notice(error.message);}}
const runningOperation=sessionStorage.getItem('slurmy-operation');if(runningOperation)watchOperation(runningOperation);

let jobs=[], taskPage=0, jobData=null, selectedOutput=null, hasLoadedRemote=false, refreshInFlight=false, staleTimer=null, lastLoadedAt=null;
function pendingJobs(){try{return JSON.parse(sessionStorage.getItem('slurmy-pending-jobs')||'[]').filter(job=>job.host===host&&Date.now()-job.created<3600000);}catch{return [];}}
function savePendingJobs(value){sessionStorage.setItem('slurmy-pending-jobs',JSON.stringify(value));}
function announceJob(id){const known=pendingJobs();if(!known.some(job=>job.id===id))known.push({id,host,directory:document.body.dataset.directory||'',name:document.querySelector('h1')?.textContent||'Slurmy',state:'SUBMITTED',total:0,completed:0,percent:0,issues:0,created:Date.now()});savePendingJobs(known);}
function remoteState(state){const panel=$('#remote-data');if(!panel)return;panel.classList.remove('loading','refreshing','stale');if(state)panel.classList.add(state);panel.setAttribute('aria-busy',String(state==='loading'||state==='refreshing'));}
function loadingRow(body,columns,message,failed=false){body.replaceChildren();const row=el('tr');row.className=failed?'loading-row failed-row':'loading-row';const cell=el('td');cell.colSpan=columns;if(!failed)cell.append(el('span','', 'spinner'));cell.append(document.createTextNode(' '+message));row.append(cell);body.append(row);}
function beginRefresh(){clearTimeout(staleTimer);remoteState(hasLoadedRemote?'refreshing':'loading');const refresh=$('#refresh');if(refresh){refresh.disabled=true;refresh.textContent='↻ Refreshing…';}if(hasLoadedRemote)text('#connection',`Refreshing data from ${host}… Previously loaded data remains visible.`);}
function finishRefresh(){remoteState('');const refresh=$('#refresh');if(refresh){refresh.disabled=false;refresh.textContent='↻ Refresh';}}
function scheduleStaleWarning(){clearTimeout(staleTimer);lastLoadedAt=new Date();staleTimer=setTimeout(()=>{if(hasLoadedRemote&&!refreshInFlight){remoteState('stale');text('#connection',`Data may be stale · last successful refresh ${lastLoadedAt.toLocaleTimeString()} · automatic refresh is delayed`);}},25000);}
function renderJobs(){
 const query=$('#job-filter')?.value.toLowerCase()||'';
 const remoteIds=new Set(jobs.map(job=>job.id));
 const pending=pendingJobs().filter(job=>!remoteIds.has(job.id)&&(!document.body.dataset.directory||job.directory===document.body.dataset.directory));
 if(pending.length)savePendingJobs(pending);
 const visible=[...pending,...jobs];
 const filtered=visible.filter(j=>`${j.id} ${j.name} ${j.state}`.toLowerCase().includes(query));
 const body=$('#jobs-body');body.replaceChildren();
 for(const job of filtered){
  const title=job.provisional?el('div',job.id,'job-link'):link(job.id,'/jobs/'+encodeURIComponent(job.id)+'?'+params({}));title.className='job-link';title.append(el('small',job.name));
  const progress=el('div');if(job.total){progress.append(el('span',`${job.completed} / ${job.total} · ${job.percent.toFixed(1)}%`,'progress-label'));const bar=el('progress');bar.max=100;bar.value=job.percent;bar.setAttribute('aria-label',`${job.percent.toFixed(1)} percent complete`);progress.append(bar);}else progress.append(el('span','Awaiting cluster metadata…','progress-label'));
  body.append(tableRow([title,badge(job.state),progress,job.issues?el('strong',job.issues,'error'):'—',new Date(job.created*1000).toLocaleString()]));
 }
 $('#empty').hidden=filtered.length!==0;
 text('#stat-total',visible.length);text('#stat-active',visible.filter(j=>['RUNNING','PENDING','SUBMITTED'].includes(j.state)).length);text('#stat-done',visible.reduce((a,j)=>a+j.completed,0).toLocaleString());text('#stat-issues',visible.reduce((a,j)=>a+j.issues,0));
}
async function fetchJobs(){const directory=document.body.dataset.directory;const data=await api('/api/jobs?'+params(directory?{directory}:{}));jobs=data.jobs;renderJobs();text('#connection',`Connected to ${host} · refreshed ${new Date(data.updated*1000).toLocaleTimeString()} · refreshes every 10 seconds`);}
async function openOutput(task){
 const panel=$('#call-output');panel.hidden=false;text('#output-title',`Call ${task.id} · ${task.system}`);text('#command',`Solver command: ${task.command}\nLimiter command: ${task.limiter_command||'Unavailable for this older job'}\nWorking directory: ${task.directory}`);$('#command').hidden=false;text('#output-info',task.problem);text('#output-text','Loading saved output…');selectedOutput=null;panel.scrollIntoView({behavior:'smooth',block:'start'});
 try{const data=await api(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/output/${task.id}?`+params({}));selectedOutput=data;renderOutput();}catch(error){text('#output-text',error.message);}
}
function renderOutput(){if(!selectedOutput)return;const stream=selectedOutput.streams[$('#stream').value];text('#output-text',stream?.content||'(This output stream is empty or was not captured.)');text('#output-info',`${stream?memory(stream.size):'0 B'}${stream?.truncated?' · Truncated: beginning and end shown':''} · Saved output for call ${selectedOutput.task_id}`);}
$('#stream')?.addEventListener('change',renderOutput);
$('#close-output')?.addEventListener('click',()=>{$('#call-output').hidden=true;selectedOutput=null;});
async function fetchJob(){
 const jobId=document.body.dataset.job;
 const data=await api(`/api/jobs/${encodeURIComponent(jobId)}?`+params({page:taskPage,q:$('#task-filter').value}));jobData=data;
 text('#connection',`Connected to ${host} · refreshed ${new Date().toLocaleTimeString()} · completion counts finished calls, not proof-search progress`);
 text('#stat-state',data.state);text('#stat-done',`${data.completed} / ${data.total}`);text('#stat-percent',data.percent.toFixed(1)+'%');text('#stat-issues',data.issues);
 const tasks=$('#tasks-body');tasks.replaceChildren();
 for(const task of data.tasks){const problem=el('span',task.problem.split('/').pop());problem.title=task.problem;tasks.append(tableRow([String(task.id),task.system||'—',problem,badge(task.state),duration(task.cpu),duration(task.wall),memory(task.memory),task.output?button('Inspect →',()=>openOutput(task)):'Not saved yet']));}
 if(!data.tasks.length)tasks.append(tableRow(['No matching calls.','','','','','','','']));
 text('#page-label',`${data.matched} matching calls · page ${taskPage+1} of ${Math.max(1,Math.ceil(data.matched/100))}`);$('#previous').disabled=taskPage===0;$('#next').disabled=(taskPage+1)*100>=data.matched;
}
async function refresh(){
 if(refreshInFlight)return;refreshInFlight=true;beginRefresh();
 try{if(page==='jobs'||page==='workflow')await fetchJobs();else if(page==='job')await fetchJob();hasLoadedRemote=true;finishRefresh();scheduleStaleWarning();}
 catch(error){remoteState('stale');const retained=hasLoadedRemote?'Previously loaded data is still shown.':'No server data has been loaded.';text('#connection',`Data is stale: ${error.message}. ${retained} Check your SSH access or VPN.`);if(!hasLoadedRemote){if(page==='job')loadingRow($('#tasks-body'),8,'Calls could not be loaded. Use Refresh to try again.',true);else loadingRow($('#jobs-body'),5,'Jobs could not be loaded. Use Refresh to try again.',true);}const refresh=$('#refresh');if(refresh){refresh.disabled=false;refresh.textContent='↻ Try again';}}
 finally{refreshInFlight=false;}
}
$('#refresh')?.addEventListener('click',refresh);
$('#job-filter')?.addEventListener('input',()=>{if(hasLoadedRemote)renderJobs();});
let searchTimer;$('#task-filter')?.addEventListener('input',()=>{taskPage=0;clearTimeout(searchTimer);searchTimer=setTimeout(refresh,300);});
$('#previous')?.addEventListener('click',()=>{taskPage--;refresh();});$('#next')?.addEventListener('click',()=>{taskPage++;refresh();});
$('#sync')?.addEventListener('click',()=>startOperation(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/action?`+params({}),{action:'sync'}));
$('#prepare')?.addEventListener('click',()=>startOperation('/api/workflow/action?'+params({directory:document.body.dataset.directory}),{action:'all'}));
$('#submit')?.addEventListener('click',()=>startOperation('/api/workflow/action?'+params({directory:document.body.dataset.directory}),{action:'submit'}));
$('#duplicate')?.addEventListener('click',async()=>{
 const suggested=(document.querySelector('h1')?.textContent||'workflow')+'-copy';
 const name=prompt('Name the workflow copy (lowercase letters, numbers, and hyphens):',suggested);
 if(name===null)return;
 try{const data=await api('/api/workflows/duplicate?'+params({directory:document.body.dataset.directory}),{name:name.trim()});location.href='/workflow?'+params({directory:data.directory});}
 catch(error){notice(error.message);}
});
$('#delete-workflow')?.addEventListener('click',async()=>{
 const name=document.querySelector('h1')?.textContent||'';
 const confirmation=prompt(`Delete “${name}”?\n\nIts submitted jobs will remain in Job history. The workflow will be moved to the local recovery archive.\n\nType the workflow name to confirm:`,'');
 if(confirmation===null)return;
 try{await api('/api/workflows/delete?'+params({directory:document.body.dataset.directory}),{name:confirmation});location.href='/workflows?'+params({deleted:name});}
 catch(error){notice(error.message);}
});
async function poll(){await refresh();setTimeout(poll,10000);}if(['jobs','job','workflow'].includes(page))poll();
