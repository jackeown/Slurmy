"""POSIX local supervisor and pair publication for macOS (also testable on Linux)."""
from __future__ import annotations
import csv
import io
import json
import math
import os
from pathlib import Path
import platform
import resource
import shlex
import signal
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import uuid

import psutil


def native_options(command):
    """Understand only the supported runsolver options; never silently ignore one."""
    words=shlex.split(command)[1:];options=dict(cpu_limit=0.,wall_limit=0.,memory_mib=0.,timestamp=False,add_eof=False)
    limits={'--cpu-limit':'cpu_limit','-C':'cpu_limit','--wall-clock-limit':'wall_limit','-W':'wall_limit','--rss-swap-limit':'memory_mib','-R':'memory_mib'}
    files={'--watcher-data','-w','--var','-v','--solver-data','-o'}
    index=0
    while index<len(words):
        word=words[index]
        if word=='--' or not word.startswith('-'):break
        flag,equals,value=word.partition('=')
        if flag in {'--timestamp','--add-eof'} and not equals:
            options['timestamp' if flag=='--timestamp' else 'add_eof']=True
        elif flag in limits or flag in files:
            if not equals:
                index+=1
                if index>=len(words):raise ValueError('Missing native supervisor option value: '+flag)
                value=words[index]
            if flag in limits:
                try:number=float(value)
                except ValueError as error:raise ValueError('Invalid native supervisor limit: '+flag) from error
                if not math.isfinite(number) or number<0:raise ValueError('Native supervisor limits must be nonnegative finite numbers.')
                options[limits[flag]]=number
        else:raise ValueError('The macOS supervisor does not support runsolver option '+flag+'. Remove it or select a native custom resource limiter.')
        index+=1
    return options


def terminate(process, known=(), grace=.5):
    """Signal the owned process group, plus descendants that made new sessions."""
    descendants=list(known)
    try:descendants.extend(psutil.Process(process.pid).children(recursive=True))
    except psutil.Error:pass
    for sig in (signal.SIGTERM,signal.SIGKILL):
        if process.returncode is None:
            try:os.killpg(process.pid,sig)
            except ProcessLookupError:pass
            except PermissionError:
                # Darwin may reject group signals once its live members have exited.
                # Signal the owned processes individually as well (including root).
                try:psutil.Process(process.pid).send_signal(sig)
                except psutil.NoSuchProcess:pass
        for child in descendants:
            try:child.send_signal(sig)
            except psutil.Error:pass
        if sig==signal.SIGTERM:time.sleep(grace)


def supervise(command, *, cwd=None, environment=None, cpu_limit=0, wall_limit=0,
              memory_mib=0, stdout=None, stderr=None, combined=None,
              timestamp=False, add_eof=False, cancel=lambda:False):
    """Measure a process tree and terminate it on a configured limit or Stop.

    Memory and live CPU are sampled; wait4 supplies the child's final Unix usage.
    Zero disables the corresponding limit. Limits do not depend on GNU tools.
    """
    started=time.monotonic()
    process=subprocess.Popen(command,cwd=cwd,env=environment,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
    lock=threading.Lock();observed={};known={};peak_rss=peak_vm=0;cpu=user=system=0.;reason='';reaped=False
    def copy(source,target):
        for line in iter(source.readline,b''):
            if target is not None:target.write(line);target.flush()
            if combined is not None:
                with lock:
                    prefix=f'{cpu:.6f}/{time.monotonic()-started:.6f}\t'.encode() if timestamp else b''
                    combined.write(prefix+line);combined.flush()
        source.close()
    readers=[threading.Thread(target=copy,args=(process.stdout,stdout),daemon=True),threading.Thread(target=copy,args=(process.stderr,stderr),daemon=True)]
    for reader in readers:reader.start()
    parent=psutil.Process(process.pid)
    try:
        while True:
            rss=vm=0
            try:current=[parent,*parent.children(recursive=True)]
            except psutil.Error:current=[]
            for child in current:
                try:
                    key=(child.pid,child.create_time());known[key]=child
                    with child.oneshot():
                        times=child.cpu_times();memory=child.memory_info()
                    observed[key]=(times.user,times.system)
                    rss+=memory.rss;vm+=memory.vms
                except psutil.NoSuchProcess:continue
                except psutil.AccessDenied as error:raise RuntimeError('Cannot measure a local solver process; refusing to run it with unverified limits.') from error
            user=sum(value[0] for value in observed.values());system=sum(value[1] for value in observed.values());cpu=user+system
            peak_rss=max(peak_rss,rss);peak_vm=max(peak_vm,vm)
            if cancel():reason='interrupted'
            elif memory_mib and rss>memory_mib*2**20:reason='memory-limit'
            elif cpu_limit and cpu>=cpu_limit:reason='time-limit'
            elif wall_limit and time.monotonic()-started>=wall_limit:reason='time-limit'
            if reason:terminate(process,known.values())
            pid,status,usage=os.wait4(process.pid,os.WNOHANG)
            if pid:
                reaped=True;process.returncode=os.waitstatus_to_exitcode(status)
                # Includes reaped descendants without double-counting sampled CPU.
                cpu=max(cpu,usage.ru_utime+usage.ru_stime)
                if usage.ru_utime+usage.ru_stime>=user+system:user,system=usage.ru_utime,usage.ru_stime
                peak_rss=max(peak_rss,int(usage.ru_maxrss*(1 if platform.system()=='Darwin' else 1024)))
                break
            time.sleep(.02)
        # Background children must not retain pipes or outlive their solver.
        if any(reader.is_alive() for reader in readers):
            for reader in readers:reader.join(.1)
        if any(reader.is_alive() for reader in readers):terminate(process,known.values())
        for reader in readers:reader.join(5)
        if any(reader.is_alive() for reader in readers):raise RuntimeError('Solver output pipes did not close after cancellation.')
    finally:
        if not reaped:
            terminate(process,known.values())
            process.wait()
    wall=time.monotonic()-started
    if not reason and cpu_limit and cpu>=cpu_limit:reason='time-limit'
    if combined is not None and add_eof:
        combined.write((f'{cpu:.6f}/{wall:.6f}\t' if timestamp else '').encode()+b'EOF\n');combined.flush()
    return dict(return_code=process.returncode if process.returncode>=0 else 128-process.returncode,
                status=reason or ('ok' if process.returncode==0 else 'solver-error'),
                wall_seconds=wall,cpu_seconds=cpu,user_seconds=user,system_seconds=system,
                cpu_usage_percent=100*cpu/wall if wall else 0,
                max_memory_kib=math.ceil(peak_rss/1024),max_virtual_memory_kib=math.ceil(peak_vm/1024),
                timed_out=reason=='time-limit',memory_out=reason=='memory-limit')


def _write_csv(path,rows,fields):
    temporary=path.with_name('.'+path.name+'.tmp')
    with temporary.open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=fields);writer.writeheader();writer.writerows(rows)
    temporary.replace(path)


def _computer_info(row):
    memory=psutil.virtual_memory()
    return '\n'.join(['% Computer : '+platform.node(),'% Model    : '+platform.machine(),
        '% CPU      : '+(platform.processor() or platform.machine()),f'% TotalCPUs : {psutil.cpu_count()}',
        f'% TotalRAM : {memory.total//2**20}MB',f'% FreeRAM  : {memory.available//2**20}MB',
        '% OS       : '+platform.platform(),f'% CPULimit : {row["cpu_limit"]}',
        f'% WCLimit  : {row["wc_limit"]}',f'% MemLimit : {math.ceil(row["mem_limit"]/2**20)}MiB',
        '% DateTime : '+time.strftime('%c %Z')])+'\n'


def run_pair(job,row,batch_id,cancel=lambda:False):
    """Run generated solver/postprocessor scripts and publish the standard archive."""
    import szs
    from postprocessing import parse_data
    from slurmy import RESULT_COLUMNS
    task=row['task_id'];call=job/'calls'/str(task);started=time.time()
    metadata=json.loads((job/'metadata.json').read_text());native=metadata.get('portable_limiter',False)
    progress=job/'progress'/f'task_{task}.csv'
    def update(state):
        temporary=progress.with_suffix('.tmp')
        with temporary.open('w',newline='') as stream:csv.writer(stream).writerow([state,task,started,time.time(),batch_id])
        temporary.replace(progress)
    update('running')
    with tempfile.TemporaryDirectory(prefix='slurmy-local-pair-') as folder:
        work=Path(folder);stem=f'task_{task:09d}_{row["task_key"]}'
        paths={name:work/(stem+suffix) for name,suffix in {
            'SOLVER_LOG':'.solver-combined.log','SOLVER_STDOUT_LOG':'.solver.log','SOLVER_STDERR_LOG':'.solver-stderr.log',
            'WATCHER_LOG':'.watcher.log','VAR_FILE':'.var','CONTROLLER_LOG':'.controller.log'}.items()}
        environment={**os.environ,**{key:str(path) for key,path in paths.items()},'JOB_DIR':str(job),'CALL_DIR':str(call),
                     'OMP_NUM_THREADS':str(row['cores']*row['threads_per_core'])}
        axioms=job/'rootfs/.slurmy-tptp'
        from local_backend import settings
        from slurmy_archives import TPTP_DEFAULT_VERSION
        default=Path(settings()['tptp_root'])/TPTP_DEFAULT_VERSION
        if axioms.is_dir():environment['TPTP']=str(axioms)
        elif default.is_dir():environment['TPTP']=str(default)
        command=['bash',str(call/('solver.sh' if native else 'limiter.sh'))]
        limiter=row['limiter_command'];options=native_options(limiter) if native else {};timestamp=options.get('timestamp',False);eof=options.get('add_eof',False)
        with paths['SOLVER_LOG'].open('wb') as combined:
            if native and metadata.get('get_computer_info',True):
                info=_computer_info(row)
                if timestamp:info=''.join('0.000000/0.000000\t'+line+'\n' for line in info.splitlines())
                combined.write(info.encode());combined.flush()
            with (work/(stem+'.limiter-stdout.log')).open('wb') as outer,paths['CONTROLLER_LOG'].open('wb') as diagnostics:
                metrics=supervise(command,cwd=job/'rootfs'/row['solver_root'].lstrip('/'),environment=environment,
                    cpu_limit=options.get('cpu_limit',0),wall_limit=options.get('wall_limit',0) if native else (row['wc_limit']+25 if row['wc_limit'] else 0),
                    memory_mib=options.get('memory_mib',0),
                    stdout=None if native else outer,stderr=None if native else diagnostics,
                    combined=combined if native else None,timestamp=timestamp,add_eof=eof,cancel=cancel)
        if not native:
            if paths['SOLVER_LOG'].stat().st_size==0:paths['SOLVER_LOG'].unlink()
            if metadata.get('get_computer_info',True):
                info=_computer_info(row)
                if paths['SOLVER_LOG'].exists():
                    original=paths['SOLVER_LOG'].read_bytes()
                    if timestamp:info=''.join('0.000000/0.000000\t'+line+'\n' for line in info.splitlines())
                    paths['SOLVER_LOG'].write_bytes(info.encode()+original)
                else:(work/(stem+'.computer-info.log')).write_text(info)
        for name in ['SOLVER_STDOUT_LOG','SOLVER_STDERR_LOG']:
            if not paths[name].exists():paths[name].touch()
        if native:
            values=dict(CPUTIME=metrics['cpu_seconds'],WCTIME=metrics['wall_seconds'],USERTIME=metrics['user_seconds'],SYSTEMTIME=metrics['system_seconds'],
                        MAXVM=metrics['max_virtual_memory_kib'],MAXRSS=metrics['max_memory_kib'],CPUUSAGE=metrics['cpu_usage_percent'],TIMEOUT=str(metrics['timed_out']).lower(),MEMOUT=str(metrics['memory_out']).lower())
            paths['VAR_FILE'].write_text(''.join(f'{key}={value}\n' for key,value in values.items()))
            paths['WATCHER_LOG'].write_text(f'Slurmy native POSIX supervisor\nChild status: {metrics["return_code"]}\nMax. memory (cumulated for all children) (KiB): {metrics["max_memory_kib"]}\n')
        else:
            values={}
            if paths['WATCHER_LOG'].is_file():
                for line in paths['WATCHER_LOG'].read_text(errors='replace').splitlines():
                    if line.startswith('Child status:') and line.split(':',1)[1].strip().isdigit():
                        metrics['return_code']=int(line.split(':',1)[1]);metrics['status']='ok' if metrics['return_code']==0 else 'solver-error'
            if paths['VAR_FILE'].is_file():values=dict(line.split('=',1) for line in paths['VAR_FILE'].read_text().splitlines() if '=' in line and not line.startswith('#'))
            for key,field in [('CPUTIME','cpu_seconds'),('WCTIME','wall_seconds'),('MAXRSS','max_memory_kib'),('MAXVM','max_virtual_memory_kib')]:
                if values.get(key):metrics[field]=float(values[key])
            if values.get('MEMOUT')=='true':metrics.update(status='memory-limit',memory_out=True)
            elif values.get('TIMEOUT')=='true':metrics.update(status='time-limit',timed_out=True)
        solver_interrupted=metrics['status']=='interrupted';data={};post_code='';post_error=''
        post=work/(stem+'.postprocessor.log');post_err=work/(stem+'.postprocessor-stderr.log')
        if not solver_interrupted and row['postprocessor_mode']!='none':
            update('postprocessing')
            with post.open('wb') as out,post_err.open('wb') as error:
                analysis=supervise(['bash',str(call/'postprocess.sh'),str(paths['SOLVER_STDOUT_LOG']),str(paths['SOLVER_STDERR_LOG'])],
                    environment=environment,wall_limit=row['postprocessor_timeout'],stdout=out,stderr=error,cancel=cancel)
            post_code=analysis['return_code']
            if analysis['status']=='interrupted':post_error='Postprocessing interrupted.'
            elif analysis['timed_out']:post_error=f'Postprocessor timed out after {row["postprocessor_timeout"]} seconds.'
            elif post_code:post_error=f'Postprocessor exited with status {post_code}.'
            data=parse_data(post.read_text(errors='replace'))
            (work/(stem+'.postprocess.json')).write_text(json.dumps(data))
        canonical=szs.canonical(data.get('status',''));output=szs.data_form(data.get('output',''))
        if not szs.mnemonic(canonical):canonical=''
        if not szs.data_mnemonic(output):output=''
        if metrics['status'] not in {'interrupted','time-limit','memory-limit'} and canonical and not (metrics['status']=='solver-error' and metrics['return_code']>=128):metrics['status']=szs.execution_status(canonical) or metrics['status']
        result={**metrics,'task_id':task,'complete':not solver_interrupted,'task_key':row['task_key'],
                'szs_status':canonical,'szs_output':output,'postprocess_data':json.dumps(data),
                'postprocess_return_code':post_code,'postprocess_error':post_error,'postprocess_status':data.get('status',''),
                'node':platform.node(),'slurm_id':f'{os.getppid()}_{batch_id}'}
        archive=f'batch_{batch_id}_{stem}_{os.getpid()}_{uuid.uuid4().hex}.tar.gz';result['archive']=archive
        temporary=job/'results'/('.'+archive+'.tmp')
        with tarfile.open(temporary,'w:gz') as target:
            for path in work.iterdir():target.add(path,arcname=path.name)
        temporary.replace(job/'results'/archive)
        # Match the Bash runner's lowercase booleans in persisted CSVs.
        for key in ['complete','timed_out','memory_out']:result[key]=str(result[key]).lower()
        _write_csv(job/'results'/f'batch_{batch_id:06d}_task_{task:09d}.csv',[result],RESULT_COLUMNS)
        index=job/'results/index'/f'batch_{batch_id:06d}.csv';previous=[]
        if index.exists():
            with index.open() as stream:previous=[item for item in csv.DictReader(stream) if int(item['task_id'])!=task]
        _write_csv(index,[*previous,result],RESULT_COLUMNS)
        update(result['status'])
        return result['complete']=='true'
