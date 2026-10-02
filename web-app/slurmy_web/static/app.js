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
const outcomeColors={'not finished':'#87938f','theorem':'#278866','unsatisfiable':'#4479b5','satisfiable':'#9367ac','countersatisfiable':'#219a98','counter satisfiable':'#219a98','timeout':'#e7b251','time limit':'#ca842c','memory limit':'#bb684a','resourceout':'#9f5241','resource out':'#9f5241','error':'#bb555b','solver error':'#bb555b'};
const otherOutcomeColors=['#6d70b7','#a66b8b','#628b51','#ac7650','#568d9c','#a5a047'];
let selectedOutcome=null;
function showOutcomeComparison(item){
 const panel=$('#status-comparison'),bars=$('#status-bars');if(!panel||!bars)return;
 selectedOutcome=`${item.label}\u0000${item.source}`;panel.hidden=false;text('#status-comparison-title',`${item.label}${item.source?` (${item.source})`:''} by solver`);
 bars.replaceChildren();const solvers=[...(item.solvers||[])].sort((a,b)=>b.count-a.count||a.name.localeCompare(b.name));const maximum=Math.max(1,...solvers.map(solver=>solver.count));
 for(const solver of solvers){const row=el('div',undefined,'status-bar-row'),track=el('div',undefined,'status-bar-track'),fill=el('span',undefined,'status-bar-fill');fill.style.width=`${solver.count/maximum*100}%`;track.append(fill);row.append(el('span',solver.name,'status-bar-label'),track,el('strong',solver.count.toLocaleString(),'status-bar-count'));bars.append(row);}
 if(!solvers.length)bars.append(el('p','No solver names are available for these calls.','muted'));
}
$('#close-status-comparison')?.addEventListener('click',()=>{$('#status-comparison').hidden=true;selectedOutcome=null;});
function renderOutcomes(outcomes){
 const chart=$('#outcome-chart'),breakdown=$('#outcome-breakdown');if(!chart||!breakdown)return;
 const items=(outcomes||[]).filter(item=>Number.isFinite(item.count)&&item.count>0);
 const total=items.reduce((sum,item)=>sum+item.count,0);
 chart.replaceChildren();breakdown.replaceChildren();chart.classList.toggle('is-empty',total===0);
 if(!total){chart.style.background='';chart.setAttribute('aria-label','No call outcomes yet');breakdown.append(el('p','No call outcomes yet.','muted'));$('#status-comparison').hidden=true;selectedOutcome=null;window.outcomeCubeData=[];window.renderOutcomeCube?.([]);return;}
 let start=0,other=0;const segments=[];const labels=[];
 for(const item of items){
  const color=outcomeColors[item.label.toLowerCase()]||otherOutcomeColors[other++%otherOutcomeColors.length];
  item.color=color;
  const end=start+item.count/total*100;segments.push(`${color} ${start}% ${end}%`);start=end;
  const percent=item.count/total*100;const row=el('button',undefined,'outcome-row outcome-row-button');row.type='button';row.setAttribute('aria-label',`Compare solvers for ${item.label}${item.source?` (${item.source})`:''}`);row.addEventListener('click',()=>showOutcomeComparison(item));
  const name=el('span',undefined,'outcome-name');const swatch=el('span',undefined,'outcome-swatch');swatch.style.backgroundColor=color;swatch.setAttribute('aria-hidden','true');name.append(swatch,el('span',item.label));if(item.source)name.append(el('small',`(${item.source})`,'outcome-source'));
  row.append(name,el('span',item.count.toLocaleString(),'outcome-count'),el('strong',`${percent.toFixed(1)}%`,'outcome-percent'));breakdown.append(row);
  labels.push(`${item.label}${item.source?` (${item.source})`:''}: ${item.count} (${percent.toFixed(1)}%)`);
 }
 chart.style.background=`conic-gradient(${segments.join(',')})`;
 chart.setAttribute('aria-label',`Call outcomes for ${total} calls. ${labels.join('; ')}`);
 chart.title=labels.join('\n');
 window.outcomeCubeData=items;window.renderOutcomeCube?.(items);
 if(selectedOutcome){const selected=items.find(item=>`${item.label}\u0000${item.source}`===selectedOutcome);if(selected)showOutcomeComparison(selected);else{$('#status-comparison').hidden=true;selectedOutcome=null;}}
}
function badge(state){const value=state.toLowerCase();let cls='';if(/error|fail|incomplete|interrupted|cancel|not run/.test(value))cls='bad';else if(/limit|timeout/.test(value))cls='limit';else if(/run|pending|submit|dispatch|completing/.test(value))cls='active';else if(/done|ok|completed|theorem|unsatisfiable|satisfiable|counter/.test(value))cls='good';return el('span',state,`badge ${cls}`);}
function text(id, value){const node=$(id);if(node)node.textContent=value;}
function button(label, action, cls='secondary'){const node=el('button',label,cls);node.type='button';node.addEventListener('click',action);return node;}
function tableRow(cells){const tr=el('tr');for(const cell of cells){const td=el('td');td.append(cell instanceof Node?cell:document.createTextNode(cell??'—'));tr.append(td);}return tr;}
const tableSort={tasks:{key:'id',direction:'asc'},problem:{key:'wall',direction:'asc'},jobs:{key:'created',direction:'desc'}};
function pageSize(id){const value=Number($(id)?.value);return Number.isInteger(value)?Math.max(1,Math.min(10000,value)):100;}
function tableKind(){return page==='job'?'tasks':page==='problem'?'problem':page==='jobs'||page==='workflow'?'jobs':null;}
function currentPage(){const kind=tableKind();return kind==='tasks'?taskPage:kind==='problem'?problemPage:jobsPage;}
function setCurrentPage(value){const kind=tableKind();if(kind==='tasks')taskPage=value;else if(kind==='problem')problemPage=value;else if(kind==='jobs')jobsPage=value;}
function syncTableUrl(push=false){
 const kind=tableKind();if(!kind)return;
 const url=new URL(location.href),sizeId=kind==='tasks'?'#task-page-size':kind==='problem'?'#problem-page-size':'#jobs-page-size';
 url.searchParams.set('page',String(currentPage()+1));url.searchParams.set('per_page',String(pageSize(sizeId)));
 url.searchParams.set('sort',tableSort[kind].key);url.searchParams.set('direction',tableSort[kind].direction);
 const filter=kind==='tasks'?$('#task-filter'):kind==='jobs'?$('#job-filter'):null;
 if(filter){if(filter.value)url.searchParams.set('q',filter.value);else url.searchParams.delete('q');}
 if(url.href!==location.href)history[push?'pushState':'replaceState'](null,'',url);
}
function restoreTableUrl(){
 const kind=tableKind();if(!kind)return;
 const query=new URLSearchParams(location.search),rawPage=Number(query.get('page'));
 setCurrentPage(Number.isInteger(rawPage)&&rawPage>0?rawPage-1:0);
 const sizeId=kind==='tasks'?'#task-page-size':kind==='problem'?'#problem-page-size':'#jobs-page-size';
 const size=Number(query.get('per_page'));if(Number.isInteger(size)&&size>=1&&size<=10000)$(sizeId).value=String(size);
 const table=document.querySelector(`table[data-sort-table="${kind}"]`);
 const key=query.get('sort');if(key&&[...table.querySelectorAll('th[data-sort]')].some(th=>th.dataset.sort===key))tableSort[kind].key=key;
 const direction=query.get('direction');if(direction==='asc'||direction==='desc')tableSort[kind].direction=direction;
 const filter=kind==='tasks'?$('#task-filter'):kind==='jobs'?$('#job-filter'):null;if(filter)filter.value=query.get('q')||'';
 updateSortButtons(table);
}
function updateSortButtons(table){
 const sort=tableSort[table.dataset.sortTable];
 table.querySelectorAll('thead th[data-sort]').forEach(th=>{
  const control=th.querySelector('.sort-button'),active=sort.key===th.dataset.sort;
  control.textContent=`${control.dataset.label} ${active?(sort.direction==='asc'?'↑':'↓'):'↕'}`;
  if(active)th.setAttribute('aria-sort',sort.direction==='asc'?'ascending':'descending');else th.removeAttribute('aria-sort');
 });
}
function initTableSorting(){
 document.querySelectorAll('table[data-sort-table]').forEach(table=>{
  table.querySelectorAll('thead th[data-sort]').forEach(th=>{
   const label=th.textContent.trim();
   th.textContent='';
   const control=el('button',label,'sort-button secondary');
   control.type='button';control.title=`Sort by ${label}`;control.setAttribute('aria-label',`Sort by ${label}`);
   control.dataset.label=label;
   control.addEventListener('click',()=>{
    const kind=table.dataset.sortTable,sort=tableSort[kind];
    sort.direction=sort.key===th.dataset.sort&&sort.direction==='asc'?'desc':'asc';sort.key=th.dataset.sort;
    updateSortButtons(table);
    if(kind==='tasks'){taskPage=0;syncTableUrl(true);refresh();}
    else if(kind==='problem'){problemPage=0;syncTableUrl(true);refresh();}
    else{jobsPage=0;syncTableUrl(true);renderJobs();}
   });
   th.append(control);
  });
  updateSortButtons(table);
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
function lockWorkflowActions(locked){if(page!=='workflow')return;for(const id of ['prepare','submit','submit-only','submit-menu-toggle']){const control=$('#'+id);if(control)control.disabled=locked;}}
async function watchOperation(id){
  clearTimeout(operationTimer);$('#operation').hidden=false;sessionStorage.setItem('slurmy-operation',id);
  try{
    const op=await api('/api/operations/'+encodeURIComponent(id));text('#operation-title',`${op.phase} · ${op.state}`);text('#operation-log',op.log||'Starting…');
    const match=op.log.match(/Slurmy job ID: ([A-Za-z0-9._-]+)/);
    $('#submitted-link')?.remove();
    if(match){announceJob(match[1],op.requested_name);renderJobs();const node=link('View submitted job →','/jobs/'+encodeURIComponent(match[1])+'?'+params({}));node.id='submitted-link';$('#operation').append(node);refresh();}
    if(op.state==='running'){lockWorkflowActions(true);operationTimer=setTimeout(()=>watchOperation(id),1500);}
    else{lockWorkflowActions(false);sessionStorage.removeItem('slurmy-operation');await refresh();}
  }catch(error){lockWorkflowActions(false);text('#operation-log',error.message);sessionStorage.removeItem('slurmy-operation');}
}
$('#dismiss-operation')?.addEventListener('click',()=>{$('#operation').hidden=true;clearTimeout(operationTimer);sessionStorage.removeItem('slurmy-operation');});
async function startOperation(url,data){try{const op=await api(url,data);lockWorkflowActions(true);watchOperation(op.id);}catch(error){notice(error.message);}}
const runningOperation=sessionStorage.getItem('slurmy-operation');if(runningOperation)watchOperation(runningOperation);

let jobs=[], taskPage=0, problemPage=0, jobsPage=0, jobData=null, selectedOutput=null, hasLoadedRemote=false, refreshInFlight=false, refreshQueued=false, staleTimer=null, lastLoadedAt=null;
restoreTableUrl();
function pendingJobs(){try{return JSON.parse(sessionStorage.getItem('slurmy-pending-jobs')||'[]').filter(job=>job.host===host&&Date.now()-job.created<3600000);}catch{return [];}}
function savePendingJobs(value){sessionStorage.setItem('slurmy-pending-jobs',JSON.stringify(value));}
async function deleteJob(id, control, redirect=false){
 control.disabled=true;
 try{
  await api(`/api/jobs/${encodeURIComponent(id)}/delete?`+params({}),{});
  savePendingJobs(pendingJobs().filter(item=>item.id!==id));
  if(redirect){location.href='/?'+params({});return;}
  jobs=jobs.filter(item=>item.id!==id);renderJobs();
  notice(`Deleted job ${id} from the cluster and local synced results.`);
 }catch(error){control.disabled=false;notice(error.message);}
}
async function renameJob(id,current){
 const proposed=prompt('Job name (1–100 characters):',current||id);
 if(proposed===null)return;
 try{
  const result=await api(`/api/jobs/${encodeURIComponent(id)}/name?`+params({}),{name:proposed});
  const item=jobs.find(job=>job.id===id);if(item){item.id=result.id;item.name=result.name;}
  savePendingJobs(pendingJobs().map(job=>job.id===id?{...job,id:result.id,name:result.name}:job));
  if(document.body.dataset.job===id){const url=new URL(location.href);url.pathname='/jobs/'+encodeURIComponent(result.id);location.href=url.href;return;}
  if($('#jobs-body'))renderJobs();
  notice(result.warning||`Job renamed to ${result.name}.`);
 }catch(error){notice(error.message);}
}
function announceJob(id,name){const known=pendingJobs();if(!known.some(job=>job.id===id))known.push({id,host,directory:document.body.dataset.directory||'',name:name||document.querySelector('h1')?.textContent||'Slurmy',state:'SUBMITTED',total:0,completed:0,percent:0,issues:0,created:Date.now()});savePendingJobs(known);}
function remoteState(state){for(const panel of [$('#remote-data'),$('#results-overview')]){if(!panel)continue;panel.classList.remove('loading','refreshing','stale');if(state)panel.classList.add(state);panel.setAttribute('aria-busy',String(state==='loading'||state==='refreshing'));}}
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
 const {key,direction}=tableSort.jobs;
 filtered.sort((a,b)=>{const av=a[key]??'',bv=b[key]??'';const result=typeof av==='number'&&typeof bv==='number'?av-bv:String(av).localeCompare(String(bv),undefined,{numeric:true});return (direction==='asc'?result:-result)||String(a.id).localeCompare(String(b.id));});
 const perPage=pageSize('#jobs-page-size');jobsPage=Math.min(jobsPage,Math.max(0,Math.ceil(filtered.length/perPage)-1));
 syncTableUrl();
 const body=$('#jobs-body');body.replaceChildren();
 for(const job of filtered.slice(jobsPage*perPage,(jobsPage+1)*perPage)){
  const title=job.provisional?el('div',job.name,'job-link'):link(job.name,'/jobs/'+encodeURIComponent(job.id)+'?'+params({}));title.className='job-link';title.append(el('small',job.id));
  const progress=el('div');if(job.total){progress.append(el('span',`${job.completed} / ${job.total} · ${job.percent.toFixed(1)}%`,'progress-label'));const bar=el('progress');bar.max=100;bar.value=job.percent;bar.setAttribute('aria-label',`${job.percent.toFixed(1)} percent complete`);progress.append(bar);}else progress.append(el('span','Awaiting cluster metadata…','progress-label'));
  if(job.phase)progress.append(el('small',job.phase,'submission-phase'));
  else if(job.submission_state==='submitting')progress.append(el('small',`Submitting batches: ${job.submitted_batches} / ${job.batch_count} accepted`,'submission-phase'));
  const cells=[title,badge(job.state),progress,job.issues?el('strong',job.issues,'error'):'—',new Date(job.created*1000).toLocaleString()];
  const actions=el('div',undefined,'job-row-actions');
  if(job.provisional)actions.append('Awaiting submission');
  else{actions.append(button('Rename',()=>renameJob(job.id,job.name)));if(page==='jobs')actions.append(button('Delete',event=>deleteJob(job.id,event.currentTarget),'danger'));}
  cells.push(actions);
  body.append(tableRow(cells));
 }
 $('#empty').hidden=filtered.length!==0;
 text('#jobs-page-label',`${filtered.length} matching jobs ·`);$('#jobs-page').value=jobsPage+1;$('#jobs-page').max=Math.max(1,Math.ceil(filtered.length/perPage));text('#jobs-page-count',Math.max(1,Math.ceil(filtered.length/perPage)));
 $('#jobs-previous').disabled=jobsPage===0;$('#jobs-next').disabled=(jobsPage+1)*perPage>=filtered.length;
 text('#stat-total',visible.length);text('#stat-active',visible.filter(j=>['RUNNING','PENDING','SUBMITTED','SUBMITTING','DISPATCHING'].includes(j.state)).length);text('#stat-done',visible.reduce((a,j)=>a+j.completed,0).toLocaleString());text('#stat-issues',visible.reduce((a,j)=>a+j.issues,0));
}
async function fetchJobs(){const directory=document.body.dataset.directory;const data=await api('/api/jobs?'+params(directory?{directory}:{}));jobs=data.jobs;renderJobs();text('#connection',`Connected to ${host} · refreshed ${new Date(data.updated*1000).toLocaleTimeString()} · refreshes every 10 seconds`);}
async function openOutput(task){
 const panel=$('#call-output');panel.hidden=false;$('#stream').closest('label').hidden=false;text('#output-title',`Call ${task.id} · ${task.solver}`);highlightShell($('#command'),`Solver command: ${task.command}\nLimiter command: ${task.limiter_command||'Unavailable for this older job'}\nWorking directory: ${task.directory}`);$('#command').hidden=false;text('#output-info',task.problem);text('#output-text','Loading saved output…');selectedOutput=null;panel.scrollIntoView({behavior:'smooth',block:'start'});
 try{const data=await api(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/output/${task.id}?`+params({}));selectedOutput=data;renderOutput();}catch(error){text('#output-text',error.message);}
}
function openDiagnostic(task){
 const panel=$('#call-output');panel.hidden=false;$('#stream').closest('label').hidden=true;
 text('#output-title',`Call ${task.id} · batch diagnostic`);
 highlightShell($('#command'),`Solver command: ${task.command}\nLimiter command: ${task.limiter_command}\nWorking directory: ${task.directory}`);
 $('#command').hidden=false;text('#output-info',task.reason);
 text('#output-text',task.diagnostic||'No Slurm log is available for this batch yet.');
 selectedOutput=null;panel.scrollIntoView({behavior:'smooth',block:'start'});
}
function renderOutput(){if(!selectedOutput)return;const stream=selectedOutput.streams[$('#stream').value];text('#output-text',stream?.exists?(stream.content||'(Empty file)'):'(No file was saved)');text('#output-info',`${stream?.explanation||'Stream unavailable.'}${stream?.exists?` · ${memory(stream.size)}`:''}${stream?.truncated?' · Truncated: beginning and end shown':''}`);}
$('#stream')?.addEventListener('change',renderOutput);
$('#close-output')?.addEventListener('click',()=>{$('#call-output').hidden=true;selectedOutput=null;});
async function fetchJob(){
 const jobId=document.body.dataset.job;
 const perPage=pageSize('#task-page-size'),requestedPage=taskPage,sort=tableSort.tasks.key,direction=tableSort.tasks.direction,query=$('#task-filter').value;
 const data=await api(`/api/jobs/${encodeURIComponent(jobId)}?`+params({page:requestedPage,per_page:perPage,sort,direction,q:query}));
 if(requestedPage!==taskPage||perPage!==pageSize('#task-page-size')||sort!==tableSort.tasks.key||direction!==tableSort.tasks.direction||query!==$('#task-filter').value){refreshQueued=true;return;}
 jobData=data;
 const lastPage=Math.max(0,Math.ceil(data.matched/perPage)-1);if(taskPage>lastPage){taskPage=lastPage;syncTableUrl();return fetchJob();}
 syncTableUrl();
 const submission=data.submission_state==='submitting'?` · submitting batches ${data.submitted_batches}/${data.batch_count}`:data.submission_state==='failed'?' · batch submission stopped before all calls were queued':'';
 text('#job-display-name',data.name);document.title=`${data.name} · Slurmy`;
 text('#connection',`Connected to ${host} · refreshed ${new Date().toLocaleTimeString()}${submission} · completion counts finished calls, not proof-search progress`);
 text('#stat-state',data.state);text('#stat-done',`${data.completed} / ${data.total}`);text('#stat-percent',data.percent.toFixed(1)+'%');text('#stat-issues',data.issues);
 renderOutcomes(data.outcomes);
 const tasks=$('#tasks-body');tasks.replaceChildren();
 for(const task of data.tasks){const problem=link(task.problem.split('/').pop(),`/jobs/${encodeURIComponent(jobId)}/problems/${task.id}?`+params({}));problem.title=task.problem;const inspect=task.output?button('Inspect →',()=>openOutput(task)):task.diagnostic?button('Inspect log →',()=>openDiagnostic(task)):'—';tasks.append(tableRow([String(task.id),task.solver||'—',problem,badge(task.state),task.will_run||'—',task.reason||'—',duration(task.cpu),duration(task.wall),memory(task.memory),inspect]));}
 if(!data.tasks.length)tasks.append(tableRow(['No matching calls.','','','','','','','','','']));
 text('#page-label',`${data.matched} matching calls ·`);$('#task-page').value=taskPage+1;$('#task-page').max=lastPage+1;text('#task-page-count',lastPage+1);$('#previous').disabled=taskPage===0;$('#next').disabled=(taskPage+1)*perPage>=data.matched;
 const selected=new URLSearchParams(location.search).get('call');if(selected&&data.tasks.some(task=>String(task.id)===selected)){const url=new URL(location.href);url.searchParams.delete('call');history.replaceState(null,'',url);openOutput(data.tasks.find(task=>String(task.id)===selected));}
 const diagnostic=new URLSearchParams(location.search).get('diagnostic');if(diagnostic&&data.tasks.some(task=>String(task.id)===diagnostic)){const url=new URL(location.href);url.searchParams.delete('diagnostic');history.replaceState(null,'',url);openDiagnostic(data.tasks.find(task=>String(task.id)===diagnostic));}
}
async function fetchProblem(){
 const jobId=document.body.dataset.job, taskId=document.body.dataset.task;
 const perPage=pageSize('#problem-page-size'),requestedPage=problemPage,sort=tableSort.problem.key,direction=tableSort.problem.direction;
 const data=await api(`/api/jobs/${encodeURIComponent(jobId)}/problems/${taskId}?`+params({page:requestedPage,per_page:perPage,sort,direction}));
 if(requestedPage!==problemPage||perPage!==pageSize('#problem-page-size')||sort!==tableSort.problem.key||direction!==tableSort.problem.direction){refreshQueued=true;return;}
 const lastPage=Math.max(0,Math.ceil(data.matched/perPage)-1);if(problemPage>lastPage){problemPage=lastPage;syncTableUrl();return fetchProblem();}
 syncTableUrl();
 text('#problem-name',data.problem.split('/').pop());text('#problem-path',data.problem);
 const source=$('#problem-text');
 if(source.tagName==='PRE'){
  source.classList.remove('output'); // Older server template used the generic log styling.
  source.classList.add('tptp-source');
 }
 if(typeof window.highlightTptp!=='function'){
  try{
   await new Promise((resolve,reject)=>{const script=document.createElement('script');script.src='/static/tptp.js';script.onload=resolve;script.onerror=reject;document.head.append(script);});
  }catch{ /* Plain text remains available if highlighting cannot load. */ }
 }
 if(typeof window.highlightTptp==='function')window.highlightTptp(source,data.text);
 else source.textContent=data.text;
 const truncated=$('#problem-truncated');if(truncated)truncated.hidden=!data.truncated;
 const body=$('#problem-tasks-body');body.replaceChildren();
 for(const task of data.tasks){const inspect=task.output?link('Inspect →',`/jobs/${encodeURIComponent(jobId)}?`+params({call:task.id})):task.diagnostic?link('Inspect log →',`/jobs/${encodeURIComponent(jobId)}?`+params({diagnostic:task.id})):'—';body.append(tableRow([String(task.id),task.solver||'—',badge(task.state),task.will_run||'—',task.reason||'—',duration(task.cpu),duration(task.wall),memory(task.memory),inspect]));}
 text('#problem-page-label',`${data.matched} calls ·`);$('#problem-page').value=problemPage+1;$('#problem-page').max=lastPage+1;text('#problem-page-count',lastPage+1);
 $('#problem-previous').disabled=problemPage===0;$('#problem-next').disabled=(problemPage+1)*perPage>=data.matched;
 text('#connection',`Connected to ${host} · refreshed ${new Date().toLocaleTimeString()} · ${data.matched} calls on this problem`);
}
async function refresh(){
 if(refreshInFlight){refreshQueued=true;return;}refreshInFlight=true;beginRefresh();
 try{if(page==='jobs'||page==='workflow')await fetchJobs();else if(page==='job')await fetchJob();else if(page==='problem')await fetchProblem();hasLoadedRemote=true;finishRefresh();scheduleStaleWarning();}
 catch(error){remoteState('stale');const retained=hasLoadedRemote?'Previously loaded data is still shown.':'No server data has been loaded.';text('#connection',`Data is stale: ${error.message}. ${retained} Check your SSH access or VPN.`);if(!hasLoadedRemote){if(page==='job')loadingRow($('#tasks-body'),10,'Calls could not be loaded. Use Refresh to try again.',true);else if(page==='problem')loadingRow($('#problem-tasks-body'),9,'Calls could not be loaded. Use Refresh to try again.',true);else if(page==='jobs'||page==='workflow')loadingRow($('#jobs-body'),6,'Jobs could not be loaded. Use Refresh to try again.',true);}const refresh=$('#refresh');if(refresh){refresh.disabled=false;refresh.textContent='↻ Try again';}}
 finally{refreshInFlight=false;if(refreshQueued){refreshQueued=false;queueMicrotask(refresh);}}
}
$('#refresh')?.addEventListener('click',refresh);
$('#job-filter')?.addEventListener('input',()=>{jobsPage=0;syncTableUrl();if(hasLoadedRemote)renderJobs();});
let searchTimer;$('#task-filter')?.addEventListener('input',()=>{taskPage=0;syncTableUrl();clearTimeout(searchTimer);searchTimer=setTimeout(refresh,300);});
for(const [id,kind] of [['#task-page-size','tasks'],['#problem-page-size','problem'],['#jobs-page-size','jobs']])$(id)?.addEventListener('change',event=>{event.currentTarget.value=pageSize(id);if(kind==='tasks'){taskPage=0;syncTableUrl(true);refresh();}else if(kind==='problem'){problemPage=0;syncTableUrl(true);refresh();}else{jobsPage=0;syncTableUrl(true);renderJobs();}});
function visitPage(inputId,lastId,kind){const input=$(inputId),last=Number($(lastId).textContent),target=Number(input.value);if(!Number.isInteger(target)||target<1||target>last){input.value=currentPage()+1;return;}setCurrentPage(target-1);syncTableUrl(true);if(kind==='jobs')renderJobs();else refresh();}
for(const [input,last,kind] of [['#task-page','#task-page-count','tasks'],['#problem-page','#problem-page-count','problem'],['#jobs-page','#jobs-page-count','jobs']])$(input)?.addEventListener('change',()=>visitPage(input,last,kind));
function stepPage(delta,kind){setCurrentPage(currentPage()+delta);syncTableUrl(true);if(kind==='jobs')renderJobs();else refresh();}
$('#previous')?.addEventListener('click',()=>stepPage(-1,'tasks'));$('#next')?.addEventListener('click',()=>stepPage(1,'tasks'));
$('#problem-previous')?.addEventListener('click',()=>stepPage(-1,'problem'));$('#problem-next')?.addEventListener('click',()=>stepPage(1,'problem'));
$('#jobs-previous')?.addEventListener('click',()=>stepPage(-1,'jobs'));$('#jobs-next')?.addEventListener('click',()=>stepPage(1,'jobs'));
window.addEventListener('popstate',()=>{restoreTableUrl();if(tableKind()==='jobs'&&hasLoadedRemote)renderJobs();else if(tableKind())refresh();});
$('#sync')?.addEventListener('click',()=>startOperation(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/action?`+params({}),{action:'sync'}));
$('#rename-job')?.addEventListener('click',()=>renameJob(document.body.dataset.job,$('#job-display-name').textContent));
$('#delete-job')?.addEventListener('click',event=>deleteJob(document.body.dataset.job,event.currentTarget,true));
const submitMenu=$('#submit-menu'),submitMenuToggle=$('#submit-menu-toggle');
function closeSubmitMenu(){if(!submitMenu)return;submitMenu.hidden=true;submitMenuToggle.setAttribute('aria-expanded','false');}
submitMenuToggle?.addEventListener('click',()=>{submitMenu.hidden=!submitMenu.hidden;submitMenuToggle.setAttribute('aria-expanded',String(!submitMenu.hidden));});
document.addEventListener('click',event=>{if(submitMenu&&!event.target.closest('.split-action'))closeSubmitMenu();});
document.addEventListener('keydown',event=>{if(event.key==='Escape'&&!submitMenu?.hidden){closeSubmitMenu();submitMenuToggle.focus();}});
function workflowAction(action){closeSubmitMenu();startOperation('/api/workflow/action?'+params({directory:document.body.dataset.directory}),{action,name:$('#new-job-name')?.value||''});}
$('#prepare')?.addEventListener('click',()=>workflowAction('prepare'));
$('#submit')?.addEventListener('click',()=>workflowAction('prepare-submit'));
$('#submit-only')?.addEventListener('click',()=>workflowAction('submit'));
$('#duplicate')?.addEventListener('click',async()=>{
 const suggested=(document.querySelector('h1')?.textContent||'workflow')+'-copy';
 const name=prompt('Name the workflow copy (lowercase letters, numbers, and hyphens):',suggested);
 if(name===null)return;
 try{const data=await api('/api/workflows/duplicate?'+params({directory:document.body.dataset.directory}),{name:name.trim()});location.href='/workflow?'+params({directory:data.directory});}
 catch(error){notice(error.message);}
});
async function deleteWorkflow(directory,name,control){
 if(!confirm(`Delete “${name}”?\n\nIts submitted jobs will remain in Job history. The workflow will be moved to the local recovery archive.`))return;
 control.disabled=true;
 try{await api('/api/workflows/delete?'+params({directory}),{name});location.href='/workflows?'+params({deleted:name});}
 catch(error){control.disabled=false;notice(error.message);}
}
$('#delete-workflow')?.addEventListener('click',event=>{
 deleteWorkflow(document.body.dataset.directory,document.querySelector('h1')?.textContent||'',event.currentTarget);
});
document.querySelectorAll('.delete-workflow-card').forEach(control=>control.addEventListener('click',()=>{
 deleteWorkflow(control.dataset.directory,control.dataset.name,control);
}));
async function poll(){await refresh();setTimeout(poll,10000);}if(['jobs','job','workflow','problem'].includes(page))poll();
