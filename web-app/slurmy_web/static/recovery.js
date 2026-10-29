'use strict';
(() => {
  const trigger=document.querySelector('#open-recovery');
  if(!trigger)return;
  const dialog=document.querySelector('#recovery-dialog');
  const list=document.querySelector('#recovery-entries');
  const message=document.querySelector('#recovery-message');
  const csrf=document.querySelector('meta[name="csrf-token"]').content;
  const node=(tag,text,cls)=>{const item=document.createElement(tag);if(text!==undefined)item.textContent=text;if(cls)item.className=cls;return item;};
  async function request(path,data){
    const response=await fetch('/api/workflows/recovery'+path,data===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:JSON.stringify(data)});
    const result=await response.json();if(!response.ok)throw Error(result.error||'Could not access the recovery archive.');return result;
  }
  const report=(text,error=false)=>{message.textContent=text;message.className=error?'error':'muted';};
  async function load(){
    report('Loading archived workflows…');list.replaceChildren();
    try{
      const data=await request('');
      report(data.entries.length?`${data.entries.length} archived workflow${data.entries.length===1?'':'s'}`:'The recovery archive is empty.');
      for(const entry of data.entries){
        const card=node('article',undefined,'recovery-entry');
        const header=node('div',undefined,'section-heading');
        const info=node('div');info.append(node('h3',entry.name),node('small','Archived '+entry.archived,'muted'));
        const actions=node('div',undefined,'tools');
        const restore=node('button','Restore');restore.type='button';
        const remove=node('button','Remove Permanently','danger');remove.type='button';
        actions.append(restore,remove);header.append(info,actions);card.append(header);
        const details=node('details'),summary=node('summary','Inspect Files'),contents=node('pre','Loading files…');
        details.append(summary,contents);let loaded=false;
        details.addEventListener('toggle',async()=>{
          if(!details.open||loaded)return;loaded=true;
          try{const inspection=await request('/'+encodeURIComponent(entry.id));contents.textContent=inspection.files.map(file=>`${file.kind==='directory'?'▸ ':file.kind==='link'?'↗ ':''}${file.path}${file.bytes===null?'':` (${file.bytes.toLocaleString()} bytes)`}`).join('\n')||'No files in this archive.';if(inspection.truncated)contents.textContent+='\n… Showing the first 300 entries. Resource links are not followed.';}
          catch(error){contents.textContent=error.message;loaded=false;}
        });
        card.append(details);list.append(card);
        restore.addEventListener('click',async()=>{
          restore.disabled=remove.disabled=true;report(`Restoring ${entry.name}…`);
          try{const result=await request('/'+encodeURIComponent(entry.id),{action:'restore'});location.href='/workflows?'+new URLSearchParams({host:document.body.dataset.host,restored:result.name});}
          catch(error){report(error.message,true);restore.disabled=remove.disabled=false;}
        });
        remove.addEventListener('click',async()=>{
          restore.disabled=remove.disabled=true;
          try{
            const choice=await SlurmyDialog.choose({title:'Remove Archived Workflow?',message:`Permanently remove “${entry.name}” and its archived files? This cannot be undone. Submitted jobs are kept.`,danger:true,choices:[{value:'remove',label:'Remove Permanently',danger:true}]});
            if(choice===null)return;
            report(`Removing ${entry.name}…`);await request('/'+encodeURIComponent(entry.id),{action:'remove',name:entry.name});await load();
          }catch(error){report(error.message,true);}finally{restore.disabled=remove.disabled=false;}
        });
      }
    }catch(error){report(error.message,true);}
  }
  trigger.addEventListener('click',event=>{event.preventDefault();event.stopPropagation();dialog.showModal();load();});
  document.querySelector('#close-recovery').addEventListener('click',()=>dialog.close());
  dialog.addEventListener('close',()=>trigger.focus());
})();
