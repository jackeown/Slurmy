'use strict';
if(['job','workflow'].includes(document.body.dataset.page)&&document.querySelector('#job-batches')){
 const panel=$('#job-batches'),list=$('#batch-list'),pagination=$('#batch-pagination');
 const prepared=document.body.dataset.page==='workflow';
 const query=values=>params({...(prepared?{directory:document.body.dataset.directory}:{}),...values});
 const jobId=document.body.dataset.job,expanded=new Map(),pairSizes=new Map();let batchPage=0,listRequest=0,retryTimer=null,focusBatch=null;
 const url=suffix=>prepared?`/api/workflow/prepared-batches${suffix}`:`/api/jobs/${encodeURIComponent(jobId)}/batches${suffix}`;
 function freshness(data){if(prepared)return `Prepared ${new Date(data.refresh.updated*1000).toLocaleString()}${data.refresh.stale?' · workflow changed since preparation':''}`;const info=data.refresh||{},when=info.updated?new Date(info.updated*1000).toLocaleTimeString():'unknown time';return info.stale?`Saved snapshot from ${when} · might be out of date${info.refreshing?' · refreshing in background':''}`:`Last refreshed ${when}`;}
 function pager(container,current,total,size,change,label,resize=null,sizeLabel=null){
  const pages=Math.max(1,Math.ceil(total/size));container.replaceChildren();
  const previous=button('Previous',()=>change(current-1)),next=button('Next',()=>change(current+1));
  previous.disabled=current===0;next.disabled=current+1>=pages;
  const jump=el('label',undefined,'page-jump'),input=el('input');input.type='number';input.min=1;input.max=pages;input.value=current+1;input.setAttribute('aria-label',label);
  input.addEventListener('change',()=>{const value=Number(input.value);if(Number.isInteger(value)&&value>=1&&value<=pages)change(value-1);else input.value=current+1;});
  jump.append(document.createTextNode('Page '),input,document.createTextNode(` of ${pages}`));
  if(resize){
   const control=el('label',sizeLabel,'page-size-control'),count=el('input');count.type='number';count.min=1;count.max=10000;count.value=size;count.setAttribute('aria-label',`${sizeLabel} (${label})`);
   count.addEventListener('change',()=>{const value=Number(count.value);if(Number.isInteger(value)&&value>=1&&value<=10000)resize(value);else{count.value=size;count.reportValidity();}});
   control.append(count);container.append(control);
  }
  container.append(previous,jump,next);
 }
 async function loadPairs(details,content,batch,pageNumber=0){
  const perPage=pairSizes.get(batch.id)||50;
  expanded.set(batch.id,pageNumber);const request=String(Number(content.dataset.request||0)+1);content.dataset.request=request;
  if(!content.childNodes.length)content.append(el('p','Loading solver-problem pairs…','muted'));
  try{
   const data=await api(url(`/${batch.id}?`)+query({page:pageNumber,per_page:perPage}));
   if(!details.isConnected||!details.open||content.dataset.request!==request)return;
   const last=Math.max(0,Math.ceil(data.total/perPage)-1);if(pageNumber>last){loadPairs(details,content,batch,last);return;}
   const table=el('table'),head=el('thead'),body=el('tbody'),scroll=el('div',undefined,'table-scroll');
   const headings=el('tr');for(const name of (prepared?['Order','Pair ID','Solver','Problem']:['Order','Pair ID','Solver','Problem','Status','Output']))headings.append(el('th',name));head.append(headings);table.append(head,body);scroll.append(table);
   for(const task of data.tasks){
    const problem=prepared?el('span',task.problem.split('/').pop()||'—'):link(task.problem.split('/').pop()||'—',`/jobs/${encodeURIComponent(jobId)}/problems/${task.id}?`+params({}));problem.title=task.problem;
    const solver=el('span',task.solver||'—');solver.title=task.command;
    const inspect=task.output?button('Inspect output',()=>openOutput(task)):(task.diagnostic||task.diagnostic_available)?button('Inspect log',()=>openDiagnostic(task)):'—';
    body.append(tableRow(prepared?[String(task.position),String(task.id),solver,problem]:[String(task.position),String(task.id),solver,problem,taskStatus(task),inspect]));
   }
   if(!data.tasks.length)body.append(tableRow(['No solver-problem pairs in this batch.','','','','','']));
   const controls=el('div',undefined,'pagination');pager(controls,pageNumber,data.total,perPage,page=>loadPairs(details,content,batch,page),`Go to pair page in batch ${batch.id}`,size=>{pairSizes.set(batch.id,size);loadPairs(details,content,batch,0);},'Solver-problem pairs per page');
   content.replaceChildren(el('p',freshness(data),'muted'),scroll,controls);
  }catch(error){if(content.dataset.request!==request||!details.isConnected)return;content.replaceChildren(el('p',error.message,'error'),button('Retry',()=>loadPairs(details,content,batch,pageNumber)));}
 }
 async function loadList(){
  clearTimeout(retryTimer);retryTimer=null;
  if(!panel.open)return;const perPage=pageSize('#batches-page-size'),request=++listRequest;
  if(!list.childNodes.length)list.append(el('p','Loading batches…','muted'));
  try{
   const data=await api(url('?')+query({page:batchPage,per_page:perPage,...(focusBatch!==null?{batch:focusBatch}:{})}));if(request!==listRequest||!panel.open)return;
   if(data.page!==undefined)batchPage=data.page;
   const last=Math.max(0,Math.ceil(data.total/perPage)-1);if(batchPage>last){batchPage=last;loadList();return;}
   text('#batch-freshness',`${data.total} batch${data.total===1?'':'es'} · ${freshness(data)}`);$('#batch-warning').hidden=prepared?!data.refresh?.stale:!data.membership_estimated;
   const children=[];
   for(const batch of data.batches){
    const details=el('details',undefined,'batch-inspector'),summary=el('summary'),content=el('div');details.id=`job-batch-${batch.id}`;
    const title=el('span',`Batch ${batch.id}`),progress=el('span',prepared?`${batch.total} solver-problem pairs · Prepared`:`${batch.finished} / ${batch.total} finished${batch.state==='Finished'?'':' · '+batch.state}`,'muted');summary.append(title,progress);
    const timings=el('span',undefined,'batch-timings');
    const formatTime=value=>value?value.replace('T',' '):'Not recorded';
    const scheduled=el('span',`Scheduled: ${formatTime(batch.scheduled)}`);scheduled.title='Time SLURM accepted this batch (Submit time). SSH timestamps are in cluster time; REST timestamps include their UTC offset.';
    const started=el('span',`Started: ${batch.started?formatTime(batch.started):['Pending','Not submitted'].includes(batch.state)?'Not started':'Not recorded'}`);
    if(!prepared){timings.append(scheduled,started);summary.append(timings);}
    const names=el('p',batch.solvers.length?`Solver configurations in this batch: ${batch.solvers.join(', ')}`:'Solver configurations unavailable','muted');
    if(batch.slurm_id)names.append(document.createTextNode(` · SLURM ${batch.slurm_id}${batch.node?' · '+batch.node:''}`));
    details.append(summary,names,content);details.addEventListener('toggle',()=>{if(details.open)loadPairs(details,content,batch,expanded.get(batch.id)||0);else expanded.delete(batch.id);});
    if(expanded.has(batch.id))details.open=true;children.push(details);
   }
   if(!children.length)children.push(el('p',data.refresh?.refreshing?'The batch plan is loading. It will appear when the job refresh completes.':'No batch plan is available for this job.','muted'));
   list.replaceChildren(...children);pager(pagination,batchPage,data.total,perPage,page=>{expanded.clear();batchPage=page;loadList();},'Go to batches page');
   if(focusBatch!==null){document.getElementById(`job-batch-${focusBatch}`)?.scrollIntoView({behavior:'smooth',block:'start'});focusBatch=null;}
   if(data.refresh?.refreshing)retryTimer=setTimeout(loadList,1000);
  }catch(error){if(request!==listRequest)return;text('#batch-freshness',`Could not update batches: ${error.message}`);if(!list.querySelector('details'))list.replaceChildren(button('Retry',loadList));}
 }
 $('#batches-page-size').addEventListener('change',event=>{event.currentTarget.value=pageSize('#batches-page-size');batchPage=0;expanded.clear();focusBatch=null;loadList();});
 panel.addEventListener('toggle',()=>{if(panel.open)loadList();else{++listRequest;clearTimeout(retryTimer);retryTimer=null;}});
 function openLinkedBatches(){
  if(location.hash!=='#job-batches')return;
  const requested=new URLSearchParams(location.search).get('batch');
  if(requested!==null&&/^\d+$/.test(requested)){focusBatch=Number(requested);expanded.set(focusBatch,0);}
  const alreadyOpen=panel.open;panel.open=true;
  if(alreadyOpen)loadList();
  panel.scrollIntoView({behavior:'smooth',block:'start'});
 }
 window.addEventListener('hashchange',openLinkedBatches);
 openLinkedBatches();
 window.SlurmyBatches={refresh:loadList};
}
