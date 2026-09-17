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
function showTab(name){document.querySelectorAll('[data-tab]').forEach(n=>n.setAttribute('aria-selected',String(n.dataset.tab===name)));document.querySelectorAll('[data-panel]').forEach(n=>n.hidden=n.dataset.panel!==name);}
document.querySelectorAll('[data-tab]').forEach(n=>n.addEventListener('click',()=>showTab(n.dataset.tab)));
$('#host-form')?.addEventListener('submit',event=>{event.preventDefault();const url=new URL(location.href);url.searchParams.set('host',$('#host').value);location.href=url;});

let operationTimer;
async function watchOperation(id){
  clearTimeout(operationTimer);$('#operation').hidden=false;sessionStorage.setItem('slurmy-operation',id);
  try{
    const op=await api('/api/operations/'+encodeURIComponent(id));text('#operation-title',`${op.label} · ${op.state}`);text('#operation-log',op.log||'Starting…');
    const match=op.log.match(/Slurmy job ID: ([A-Za-z0-9._-]+)/);
    $('#submitted-link')?.remove();
    if(match){const node=link('View submitted job →','/jobs/'+encodeURIComponent(match[1])+'?'+params({}));node.id='submitted-link';$('#operation').append(node);}
    if(op.state==='running')operationTimer=setTimeout(()=>watchOperation(id),1500);
    else{sessionStorage.removeItem('slurmy-operation');await refresh();}
  }catch(error){text('#operation-log',error.message);sessionStorage.removeItem('slurmy-operation');}
}
$('#dismiss-operation')?.addEventListener('click',()=>{$('#operation').hidden=true;clearTimeout(operationTimer);sessionStorage.removeItem('slurmy-operation');});
async function startOperation(url,data){try{const op=await api(url,data);watchOperation(op.id);}catch(error){notice(error.message);}}
const runningOperation=sessionStorage.getItem('slurmy-operation');if(runningOperation)watchOperation(runningOperation);

let jobs=[], taskPage=0, jobData=null, selectedOutput=null;
function renderJobs(){
 const query=$('#job-filter')?.value.toLowerCase()||'';
 const filtered=jobs.filter(j=>`${j.id} ${j.name} ${j.state}`.toLowerCase().includes(query));
 const body=$('#jobs-body');body.replaceChildren();
 for(const job of filtered){
  const title=link(job.id,'/jobs/'+encodeURIComponent(job.id)+'?'+params({}));title.className='job-link';title.append(el('small',job.name));
  const progress=el('div');progress.append(el('span',`${job.completed} / ${job.total} · ${job.percent.toFixed(1)}%`,'progress-label'));const bar=el('progress');bar.max=100;bar.value=job.percent;bar.setAttribute('aria-label',`${job.percent.toFixed(1)} percent complete`);progress.append(bar);
  body.append(tableRow([title,badge(job.state),progress,job.issues?el('strong',job.issues,'error'):'—',new Date(job.created*1000).toLocaleString()]));
 }
 $('#empty').hidden=filtered.length!==0;
 text('#stat-total',jobs.length);text('#stat-active',jobs.filter(j=>['RUNNING','PENDING','SUBMITTED'].includes(j.state)).length);text('#stat-done',jobs.reduce((a,j)=>a+j.completed,0).toLocaleString());text('#stat-issues',jobs.reduce((a,j)=>a+j.issues,0));
}
async function fetchJobs(){const directory=document.body.dataset.directory;const data=await api('/api/jobs?'+params(directory?{directory}:{}));jobs=data.jobs;renderJobs();text('#connection',`Connected to ${host} · refreshed ${new Date(data.updated*1000).toLocaleTimeString()} · refreshes every 10 seconds`);}
async function openOutput(task){
 showTab('output');text('#output-title',`Call ${task.id} · ${task.system}`);text('#command',`${task.command}\nWorking directory: ${task.directory}`);$('#command').hidden=false;text('#output-info',task.problem);text('#output-text','Loading saved output…');selectedOutput=null;
 try{const data=await api(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/output/${task.id}?`+params({}));selectedOutput=data;renderOutput();}catch(error){text('#output-text',error.message);}
}
function renderOutput(){if(!selectedOutput)return;const stream=selectedOutput.streams[$('#stream').value];text('#output-text',stream?.content||'(This output stream is empty or was not captured.)');text('#output-info',`${stream?memory(stream.size):'0 B'}${stream?.truncated?' · Truncated: beginning and end shown':''} · Saved output for call ${selectedOutput.task_id}`);}
$('#stream')?.addEventListener('change',renderOutput);
async function fetchJob(){
 const jobId=document.body.dataset.job;
 const data=await api(`/api/jobs/${encodeURIComponent(jobId)}?`+params({page:taskPage,q:$('#task-filter').value}));jobData=data;
 text('#connection',`Connected to ${host} · refreshed ${new Date().toLocaleTimeString()} · completion counts finished calls, not proof-search progress`);
 text('#stat-state',data.state);text('#stat-done',`${data.completed} / ${data.total}`);text('#stat-percent',data.percent.toFixed(1)+'%');text('#stat-issues',data.issues);
 const tasks=$('#tasks-body');tasks.replaceChildren();
 for(const task of data.tasks){const problem=el('span',task.problem.split('/').pop());problem.title=task.problem;tasks.append(tableRow([String(task.id),task.system||'—',problem,badge(task.state),duration(task.cpu),duration(task.wall),memory(task.memory),task.output?button('Inspect →',()=>openOutput(task)):'Not saved yet']));}
 if(!data.tasks.length)tasks.append(tableRow(['No matching calls.','','','','','','','']));
 text('#page-label',`${data.matched} matching calls · page ${taskPage+1} of ${Math.max(1,Math.ceil(data.matched/100))}`);$('#previous').disabled=taskPage===0;$('#next').disabled=(taskPage+1)*100>=data.matched;
 const slurm=$('#slurm-body');slurm.replaceChildren();
 for(const record of data.slurm){const cancel=record.source==='queue'?button('Cancel',()=>{if(confirm(`Cancel Slurm job ${record.array_job_id}? Its active calls will be stopped.`))startOperation(`/api/jobs/${encodeURIComponent(jobId)}/action?`+params({}),{action:'cancel',slurm_id:record.array_job_id});},'danger'):'';slurm.append(tableRow([record.display_id,badge(record.state),record.elapsed,record.location,record.source,cancel]));}
 const logs=$('#logs');logs.replaceChildren();for(const [name,content] of Object.entries(data.logs)){const details=el('details');details.append(el('summary',name),el('pre',content));logs.append(details);}if(!Object.keys(data.logs).length)logs.append(el('p','No scheduler logs available yet.','muted'));text('#metadata',JSON.stringify(data.metadata,null,2));
}
async function refresh(){try{if(page==='jobs'||page==='experiment')await fetchJobs();else if(page==='job')await fetchJob();}catch(error){text('#connection',`Could not refresh: ${error.message}. Check your SSH access or VPN. Previously loaded data is retained.`);}}
$('#refresh')?.addEventListener('click',refresh);
$('#job-filter')?.addEventListener('input',renderJobs);
let searchTimer;$('#task-filter')?.addEventListener('input',()=>{taskPage=0;clearTimeout(searchTimer);searchTimer=setTimeout(refresh,300);});
$('#previous')?.addEventListener('click',()=>{taskPage--;refresh();});$('#next')?.addEventListener('click',()=>{taskPage++;refresh();});
$('#sync')?.addEventListener('click',()=>startOperation(`/api/jobs/${encodeURIComponent(document.body.dataset.job)}/action?`+params({}),{action:'sync'}));
$('#prepare')?.addEventListener('click',()=>startOperation('/api/experiment/action?'+params({directory:document.body.dataset.directory}),{action:'all'}));
$('#submit')?.addEventListener('click',()=>{if(confirm('Build resources on the cluster and submit this experiment? Build outputs may replace files in the declared resource directories.'))startOperation('/api/experiment/action?'+params({directory:document.body.dataset.directory}),{action:'submit'});});
async function poll(){await refresh();setTimeout(poll,10000);}if(['jobs','job','experiment'].includes(page))poll();
