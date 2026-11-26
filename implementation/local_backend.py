"""Native local execution, sharing the cluster runner and result formats."""
from __future__ import annotations
import argparse
import csv
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import threading
import uuid

REPO = Path(__file__).resolve().parents[1]
LOCAL_HOST = 'local'


def root():
    return Path(os.environ.get('SLURMY_LOCAL_ROOT', REPO/'web-app/.state/local-backend'))


def read_json(path, default=None):
    try:return json.loads(Path(path).read_text())
    except FileNotFoundError:return {} if default is None else default


def atomic_json(path, value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    fd,name=tempfile.mkstemp(prefix='.'+path.name,dir=path.parent)
    try:
        with os.fdopen(fd,'w') as stream:json.dump(value,stream)
        os.replace(name,path)
    finally:Path(name).unlink(missing_ok=True)


def process_library():
    try:import psutil
    except ImportError as error:raise ValueError('macOS local execution needs psutil. Install the project requirements: python3 -m pip install -r requirements.txt') from error
    return psutil


def affinity_command(logical, command):
    return ['taskset','-c',logical,*command] if platform.system()=='Linux' else list(command)


def cpu_groups():
    """Physical cores and their available logical CPUs, respecting affinity."""
    if platform.system()=='Darwin':
        library=process_library();physical=library.cpu_count(logical=False) or 1;logical=library.cpu_count() or physical
        threads=max(1,logical//physical)
        return {(0,index):list(range(index*threads,(index+1)*threads)) for index in range(physical)}
    allowed=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else list(range(os.cpu_count() or 1))
    groups={}
    for cpu in allowed:
        directory=Path(f'/sys/devices/system/cpu/cpu{cpu}/topology')
        try:key=(int((directory/'physical_package_id').read_text()),int((directory/'core_id').read_text()))
        except OSError:key=(0,cpu)
        groups.setdefault(key,[]).append(cpu)
    return groups


def topology():
    groups=cpu_groups();sockets={key[0] for key in groups}
    return dict(sockets=len(sockets),cores_per_socket=min(sum(key[0]==socket for key in groups) for socket in sockets),
                threads_per_core=min(map(len,groups.values())),physical_cores=len(groups),name=platform.node())


def memory_mib():
    if platform.system()=='Darwin':return max(1,process_library().virtual_memory().total//2**20)
    return max(1,os.sysconf('SC_PAGE_SIZE')*os.sysconf('SC_PHYS_PAGES')//2**20)


def settings():
    saved=read_json(root()/'settings.json')
    return dict(mode='local',platform=platform.system(),cpu_pinning=platform.system()=='Linux',max_cores=min(int(saved.get('max_cores',len(cpu_groups()))),len(cpu_groups())),
                max_parallel_batches=int(saved.get('max_parallel_batches',max(1,len(cpu_groups())//2))),
                memory_mib=int(saved.get('memory_mib',max(1,memory_mib()*3//4))),
                tptp_root=saved.get('tptp_root',str(Path.home()/'TPTP')))


def save_settings(value):
    current=settings()
    for key,maximum in [('max_cores',len(cpu_groups())),('max_parallel_batches',10000),('memory_mib',memory_mib())]:
        number=str(value.get(key,current[key]))
        if not number.isdecimal() or not 1<=int(number)<=maximum:raise ValueError(f'{key}: enter a whole number from 1 to {maximum}.')
        current[key]=int(number)
    path=Path(value.get('tptp_root') or current['tptp_root']).expanduser()
    if not path.is_absolute():raise ValueError('Local TPTP root must be an absolute directory path.')
    current['tptp_root']=str(path)
    if any(alive(read_json(job_path(item['id'])/'controller.json')) for item in jobs()) and any(current[key]!=settings()[key] for key in ('max_cores','max_parallel_batches','memory_mib')):
        raise ValueError('Stop active local jobs before changing their resource budgets.')
    atomic_json(root()/'settings.json',current)
    return current


def job_path(identifier):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*',identifier):raise ValueError('Invalid local job ID.')
    path=root()/'jobs'/identifier
    if path.is_symlink():raise ValueError('Local job directory must not be a symbolic link.')
    return path


def jobs():
    result=[]
    for path in (root()/'jobs').glob('*'):
        if path.is_dir() and not path.is_symlink():
            metadata=read_json(path/'metadata.json')
            if metadata:result.append(dict(id=path.name,name=metadata.get('job_name',path.name),submitted=metadata.get('created_epoch',path.stat().st_mtime)))
    return sorted(result,key=lambda job:(job['submitted'],job['id']),reverse=True)


def process_identity(pid):
    if platform.system()=='Darwin':
        library=process_library()
        try:
            process=library.Process(pid)
            return None if process.status()==library.STATUS_ZOMBIE else str(process.create_time())
        except library.Error:return None
    try:
        content=Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()
        return None if content[0]=='Z' else content[19]
    except (OSError,IndexError):return None


def alive(state):
    return bool(state.get('pid') and state.get('identity') and process_identity(state['pid'])==state['identity'])


def require_tools():
    system=platform.system()
    if system not in {'Linux','Darwin'}:raise ValueError('Native local execution supports Linux and macOS; Windows is not supported.')
    if system=='Darwin':process_library()
    tools=('bash','taskset','setsid','timeout','tar','awk','tee') if system=='Linux' else ('bash','tee')
    missing=[name for name in tools if shutil.which(name) is None]
    if missing:raise ValueError('Missing local execution tools: '+', '.join(missing))


def prepare(workflow):
    require_tools()
    sys.path.insert(0,str(REPO/'web-app'))
    from slurmy_web import workflows
    from slurmy import generate
    workflow=Path(workflow).resolve()
    data=workflows.decisions(workflow);data['host']='local'
    # A local run never applies cluster accounts, reservations or partition policy.
    data.update(account='',reservation='',partition='local')
    _,files,_=workflows.specification(data,workflow)
    with tempfile.TemporaryDirectory(prefix='.local-prepare-',dir=workflow) as folder:
        stage=Path(folder)
        for name,content in files.items():(stage/name).write_text(content)
        generate(stage/'solver-problem-pairs.csv',stage/'building.json',stage/'resource-limiter.txt',
                 stage/'axiom-globs.txt' if 'axiom-globs.txt' in files else None,
                 int(data.get('batch_size') or 1),host='local',partition='local',get_computer_info=data.get('get_computer_info',True))
        assets=stage/'submit.sh.files'
        from slurmy_archives import TPTP_DEFAULT_VERSION
        tptp=Path(settings()['tptp_root'])/TPTP_DEFAULT_VERSION
        for config in (assets/'calls').glob('*/config.sh'):
            config.write_text(re.sub(r'(?m)^TPTP_DEFAULT_ROOT=.*$', 'TPTP_DEFAULT_ROOT='+shlex.quote(str(tptp) if tptp.is_dir() else ''), config.read_text()))
        metadata=read_json(assets/'metadata.json');metadata.update(backend='local',workflow_directory=str(workflow),workflow_name=data['name'])
        limiter=Path(shlex.split(files['resource-limiter.txt'])[0])
        portable=platform.system()=='Darwin' and limiter.name.startswith('runsolver')
        if portable:
            from portable_runtime import native_options
            for line in (assets/'manifest.jsonl').read_text().splitlines():native_options(json.loads(line)['limiter_command'])
        metadata.update(execution_platform=platform.system(),portable_limiter=portable,local_limiter_path=str(limiter))
        atomic_json(assets/'metadata.json',metadata)
        builds=read_json(stage/'building.json',[])
        if portable:
            bundled=REPO/'implementation/build/runsolver'
            builds=[build for build in builds if Path(build.get('root','')).resolve()!=bundled.resolve() and (Path(build.get('root',''))/build.get('artifact','')).resolve()!=limiter.resolve()]
        atomic_json(assets/'local-builds.json',builds)
        for name in files:
            if name.startswith('build-resource-'):
                shutil.copy2(stage/name,assets/name)
        # Preparation replaces a plan, never a dispatched job's committed inputs.
        old=workflow/'submit.sh.files'
        if old.exists():shutil.rmtree(old)
        assets.rename(old)
        (workflow/'submit.sh').write_text('#!/usr/bin/env bash\nexec '+shlex.quote(sys.executable)+' '+shlex.quote(str(Path(__file__).resolve()))+' dispatch '+shlex.quote(str(workflow))+'\n')
    print('Local batches prepared. No job has been started.',flush=True)


def fingerprint(build):
    digest=hashlib.sha256(repr(build).encode())
    for base in sorted({Path(build.get('source') or build['root']),Path(build['recipe'])}):
        paths=[base] if base.is_file() else base.rglob('*')
        for path in sorted(paths):
            if '.git' in path.parts or path.name.startswith('.slurmy') or not path.is_file():continue
            stat=path.stat();digest.update(repr((str(path),stat.st_size,stat.st_mtime_ns)).encode())
    return digest.hexdigest()


def reserve_build(job,state):
    if not any(build.get('recipe') for build in read_json(job/'local-builds.json',[])):return
    while True:
        if (job/'stop-requested').exists():raise ValueError('Local build stopped before it started.')
        with (root()/'.scheduler.lock').open('a+') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            occupied=False
            for item in jobs():
                other=job_path(item['id'])
                if other==job:continue
                controller=alive(read_json(other/'controller.json'))
                if any(controller or alive(active) for active in read_json(other/'local-state.json').get('active',{}).values()):occupied=True;break
            if not occupied:
                limits=settings()
                state['active']={'build':dict(cores=[cpus[0] for cpus in list(cpu_groups().values())[:limits['max_cores']]],memory=limits['memory_mib'])}
                atomic_json(job/'local-state.json',state)
                return
        time.sleep(.2)


def build_resources(job):
    for index,build in enumerate(read_json(job/'local-builds.json',[])):
        if not build.get('recipe'):continue
        runtime=Path(build['root']);runtime.mkdir(parents=True,exist_ok=True)
        receipt=runtime/'.slurmy-local-build.json'
        artifact=runtime/(build.get('artifact') or '.slurmy-built')
        before=fingerprint(build)
        if build.get('build_policy')=='changed' and artifact.is_file() and read_json(receipt).get('signature')==before:
            print(f'Reusing local build: {runtime}',flush=True);continue
        print(f'Building locally: {runtime}',flush=True)
        recipe=Path(build['recipe'])
        if recipe.name.startswith('build-resource-') and (job/recipe.name).is_file():recipe=job/recipe.name
        context=Path(build.get('source') or build['root'])
        with tempfile.TemporaryDirectory(prefix='slurmy-local-build-') as folder:
            work=Path(folder)/'source';shutil.copytree(context,work,symlinks=True)
            inplace=context.resolve()==runtime.resolve()
            output=work if inplace else runtime
            environment={**os.environ,'SLURMY_BUILD_OUTPUT':str(output),'SLURMY_BUILD_WORK':str(work),'SLURMY_BUILD_SOURCE':str(work),
                         'SLURM_CPUS_PER_TASK':str(settings()['max_cores']),'SLURMY_BUILD_CORES':str(settings()['max_cores']),
                         'MAKEFLAGS':f'-j{settings()["max_cores"]}','CMAKE_BUILD_PARALLEL_LEVEL':str(settings()['max_cores'])}
            if os.access(recipe,os.X_OK):command=[str(recipe)]
            else:
                with recipe.open('rb') as stream:first=stream.readline(4096)
                interpreter=shlex.split(first[2:].decode().strip()) if first.startswith(b'#!') else ['bash']
                command=[*interpreter,str(recipe)]
            command+=build.get('arguments',[])
            logical=','.join(str(cpu) for cpus in list(cpu_groups().values())[:settings()['max_cores']] for cpu in cpus[:1])
            process=subprocess.Popen(affinity_command(logical,command),cwd=work,env=environment,start_new_session=True)
            try:
                while process.poll() is None:
                    if (job/'stop-requested').exists():
                        os.killpg(process.pid,signal.SIGTERM)
                        try:process.wait(timeout=5)
                        except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
                        raise ValueError('Local build stopped.')
                    time.sleep(.1)
                if process.returncode:raise ValueError(f'Local build exited with status {process.returncode}: {recipe}')
            finally:
                if process.poll() is None:os.killpg(process.pid,signal.SIGKILL);process.wait()
            if inplace:shutil.copytree(work,runtime,dirs_exist_ok=True,symlinks=True)
        if not build.get('artifact'):artifact.touch()
        if not artifact.is_file():raise ValueError(f'Local build did not create its expected artifact: {artifact}')
        atomic_json(receipt,dict(signature=fingerprint(build)))


def commit_inputs(job):
    build_resources(job)
    if platform.system()=='Darwin':
        sys.path.insert(0,str(REPO/'web-app'))
        from slurmy_web.workflows import validate_solver_command
        for row in (json.loads(line) for line in (job/'manifest.jsonl').read_text().splitlines()):
            executable=Path(validate_solver_command(row['command'],row['solver_root'])['path'])
            with executable.open('rb') as stream:signature=stream.read(4)
            if signature==b'\x7fELF':raise ValueError(f'{executable} is a Linux executable. Select a macOS binary or build this solver locally from source.')
        metadata=read_json(job/'metadata.json')
        if not metadata.get('portable_limiter'):
            executable=Path(metadata['local_limiter_path'])
            with executable.open('rb') as stream:signature=stream.read(4)
            if signature==b'\x7fELF':raise ValueError('The custom resource limiter is a Linux executable. Select a native macOS limiter or the bundled runsolver setting.')
    mirror=job/'rootfs';mirror.mkdir(exist_ok=True)
    raw=(job/'archive-paths.txt').read_bytes().split(b'\0')
    paths=[Path('/'+os.fsdecode(value)) for value in raw if value]
    for path in paths:
        if (job/'stop-requested').exists():raise ValueError('Local input preparation stopped.')
        target=mirror/str(path).lstrip('/')
        target.parent.mkdir(parents=True,exist_ok=True)
        if path.is_dir():shutil.copytree(path,target,dirs_exist_ok=True,symlinks=False,ignore=shutil.ignore_patterns('.git','.slurmy-local-build.json'))
        else:shutil.copy2(path,target)
    if (job/'prepare_axioms.py').exists():
        subprocess.run([sys.executable,str(job/'prepare_axioms.py')],env={**os.environ,'JOB_DIR':str(job)},check=True)


def dispatch(workflow,name=None):
    require_tools()
    from job_names import unused_job_name
    workflow=Path(workflow).resolve();assets=workflow/'submit.sh.files'
    metadata=read_json(assets/'metadata.json')
    if metadata.get('backend')!='local':raise ValueError('Prepare this workflow with the local backend before dispatching it.')
    if metadata.get('execution_platform',platform.system())!=platform.system():raise ValueError('Prepare these local batches again on this operating system before dispatching them.')
    rows=[json.loads(line) for line in (assets/'manifest.jsonl').read_text().splitlines()]
    capacity=settings()
    for row in rows:
        if row['cores']>capacity['max_cores']:raise ValueError('A solver requests more cores than the local execution budget. Increase the local budget or reduce solver cores.')
        if row['mem_limit'] and math.ceil(row['mem_limit']/2**20)>capacity['memory_mib']:raise ValueError('A solver memory limit exceeds the local execution budget.')
    identifier=workflow.name+'_'+str(int(time.time()))+'_'+uuid.uuid4().hex[:10]
    job=job_path(identifier);job.parent.mkdir(parents=True,exist_ok=True)
    shutil.copytree(assets,job)
    for directory in ('logs','progress','results/index'):(job/directory).mkdir(parents=True,exist_ok=True)
    metadata.update(job_id=identifier,job_name=name or unused_job_name(metadata.get('workflow_name',workflow.name),{item['name'] for item in jobs()}),created_epoch=time.time(),backend='local')
    atomic_json(job/'metadata.json',metadata)
    atomic_json(job/'local-state.json',dict(state='starting',active={}))
    launch(job)
    print('Slurmy job ID: '+identifier,flush=True)
    return identifier


def launch(job):
    with (job/'logs/submission-controller.log').open('a') as log:
        process=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'worker',job.name],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
    atomic_json(job/'controller.json',dict(pid=process.pid,identity=process_identity(process.pid)))
    threading.Thread(target=process.wait,daemon=True).start()


def results(job):
    from remote_export import records
    return records(job)


def batch(job,batch_id,logical):
    metadata=read_json(job/'metadata.json')
    stop=False;child=None
    def cancel(*_):
        nonlocal stop
        stop=True
        if child and child.poll() is None:os.killpg(child.pid,signal.SIGTERM)
    signal.signal(signal.SIGTERM,cancel);signal.signal(signal.SIGINT,cancel)
    rows={row['task_id']:row for row in (json.loads(line) for line in (job/'manifest.jsonl').read_text().splitlines())} if platform.system()=='Darwin' else {}
    for task in metadata['planned_batch_tasks'][batch_id]:
        if stop or (job/'stop-requested').exists():break
        if str(results(job).get(task,{}).get('complete','')).lower()=='true':continue
        if platform.system()=='Darwin':
            from portable_runtime import run_pair
            if not run_pair(job,rows[task],batch_id,cancel=lambda:stop or (job/'stop-requested').exists()):return 130
            continue
        environment={**os.environ,'JOB_DIR':str(job),'SLURM_JOB_ID':str(os.getppid()),'SLURM_ARRAY_JOB_ID':str(os.getppid()),
                     'SLURM_ARRAY_TASK_ID':str(batch_id),'SLURMD_NODENAME':platform.node()}
        child=subprocess.Popen(affinity_command(logical,['bash',str(job/'call.sh'),str(task),str(batch_id)]),env=environment,start_new_session=True)
        child.wait()
        if child.returncode and not stop:
            # A runner error must remain visible and resumable, not spin forever.
            print(f'Pair {task} did not finish; resume the job after correcting its execution error.',flush=True)
            return 1
    return 130 if stop or (job/'stop-requested').exists() else 0


def worker(identifier):
    job=job_path(identifier)
    with (job/'.controller.lock').open('a+') as job_lock:
        try:fcntl.flock(job_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:return
        state=read_json(job/'local-state.json');children={};stopping=False
        def cancel(*_):
            nonlocal stopping
            stopping=True
            (job/'stop-requested').touch()
        signal.signal(signal.SIGTERM,cancel);signal.signal(signal.SIGINT,cancel)
        try:
            if not (job/'inputs-ready').exists():
                state.update(state='building',active={});atomic_json(job/'local-state.json',state)
                reserve_build(job,state)
                commit_inputs(job);(job/'inputs-ready').touch()
                state['active']={};atomic_json(job/'local-state.json',state)
            metadata=read_json(job/'metadata.json');rows=[json.loads(line) for line in (job/'manifest.jsonl').read_text().splitlines()]
            requests={i:dict(cores=max(rows[t]['cores'] for t in ids),reserved=settings()['max_cores'] if any(rows[t]['exclusive_cpu'] or rows[t]['exclusive_node'] for t in ids) else max(rows[t]['cores'] for t in ids),threads=max(rows[t]['threads_per_core'] for t in ids),
                             memory=max(math.ceil(rows[t]['mem_limit']/2**20) or settings()['memory_mib'] for t in ids))
                      for i,ids in enumerate(metadata['planned_batch_tasks'])}
            attempted=set();finished=set()
            while True:
                stopping=stopping or (job/'stop-requested').exists()
                for bid,(process,started) in list(children.items()):
                    if process.poll() is not None:
                        del children[bid]
                        allocation=state['active'].pop(str(bid),{})
                        state.setdefault('history',{})[str(bid)]={**allocation,'ended':time.time(),'return_code':process.returncode}
                    elif stopping:
                        process.terminate()
                        try:process.wait(timeout=12)
                        except subprocess.TimeoutExpired:process.kill();process.wait()
                if stopping and not children:break
                finished={task for task,row in results(job).items() if str(row.get('complete','')).lower()=='true'}
                pending=[i for i,ids in enumerate(metadata['planned_batch_tasks']) if i not in attempted and any(t not in finished for t in ids)]
                if not pending and not children:break
                with (root()/'.scheduler.lock').open('a+') as lock:
                    fcntl.flock(lock,fcntl.LOCK_EX)
                    used=set();memory=0;running=0
                    for other in jobs():
                        other_path=job_path(other['id'])
                        controller_live=alive(read_json(other_path/'controller.json'))
                        for active in read_json(other_path/'local-state.json').get('active',{}).values():
                            if not controller_live and not alive(active):continue
                            used.update(active['cores']);memory+=active['memory'];running+=1
                    limits=settings();groups=cpu_groups();available=[key for key in groups if groups[key][0] not in used][:max(0,limits['max_cores']-len(used))]
                    if not stopping:
                        for bid in pending:
                            request=requests[bid]
                            if running>=limits['max_parallel_batches']:break
                            if len(available)<request['reserved'] or memory+request['memory']>limits['memory_mib']:continue
                            selected=available[:request['reserved']]
                            if any(len(groups[key])<request['threads'] for key in selected):raise ValueError('Requested hardware threads are unavailable on the selected local cores.')
                            logical=','.join(str(cpu) for key in selected[:request['cores']] for cpu in groups[key][:request['threads']])
                            with (job/f'logs/local-batch-{bid}.log').open('a') as log:
                                process=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'batch',identifier,str(bid),logical],stdout=log,stderr=subprocess.STDOUT)
                            started=time.time();children[bid]=(process,started);attempted.add(bid)
                            state.setdefault('active',{})[str(bid)]=dict(pid=process.pid,identity=process_identity(process.pid),parent=os.getpid(),solver_cores=request['cores'],cores=[groups[key][0] for key in selected],memory=request['memory'],started=started)
                            available=available[request['reserved']:];memory+=request['memory'];running+=1
                    state.update(state='running' if children else 'pending')
                    atomic_json(job/'local-state.json',state)
                time.sleep(.2)
            state.update(state='stopped' if stopping else 'done' if len(finished)==metadata['task_count'] else 'incomplete',active={})
        except BaseException as error:
            print(f'Local execution failed: {error}',flush=True)
            for process,_ in children.values():
                if process.poll() is None:process.terminate()
            for process,_ in children.values():
                try:process.wait(timeout=15)
                except subprocess.TimeoutExpired:process.kill();process.wait()
            state.update(state='stopped' if (job/'stop-requested').exists() else 'incomplete',active={},error=str(error))
        atomic_json(job/'local-state.json',state)


def action(action,identifier):
    job=job_path(identifier)
    if not job.is_dir():raise ValueError('Local job not found.')
    if action in {'stop','delete'}:
        (job/'stop-requested').touch()
        if not alive(read_json(job/'controller.json')):
            for active in read_json(job/'local-state.json').get('active',{}).values():
                if alive(active):os.kill(active['pid'],signal.SIGTERM)
            deadline=time.monotonic()+15
            while any(alive(active) for active in read_json(job/'local-state.json').get('active',{}).values()):
                if time.monotonic()>deadline:raise ValueError('Orphaned local processes are still stopping; job files were kept.')
                time.sleep(.1)
        deadline=time.monotonic()+90
        while alive(read_json(job/'controller.json')):
            if time.monotonic()>deadline:raise ValueError('The local job is still stopping; its files were kept. Retry after its build or current pair exits.')
            time.sleep(.1)
        state=read_json(job/'local-state.json');state.update(state='stopped',active={});atomic_json(job/'local-state.json',state)
    if action=='delete':shutil.rmtree(job)
    elif action=='resume':
        if alive(read_json(job/'controller.json')):return
        action_fn=globals()['action']
        action_fn('stop',identifier)
        (job/'stop-requested').unlink(missing_ok=True)
        launch(job)


def copy_results(identifier,destination,results_only=False):
    source=job_path(identifier);destination=Path(destination);destination.mkdir(parents=True,exist_ok=True)
    names=['metadata.json','manifest.jsonl','results']
    if not results_only:names+=['plans','progress','logs','allocations.csv','submission.csv']
    count=0;expected=set()
    for name in names:
        base=source/name
        paths=[base] if base.is_file() else base.rglob('*') if base.is_dir() else []
        for path in paths:
            if not path.is_file() or path.name.startswith('.'):continue
            relative=path.relative_to(source);expected.add(relative)
            target=destination/relative
            if not target.is_file() or (target.stat().st_size,target.stat().st_mtime_ns)!=(path.stat().st_size,path.stat().st_mtime_ns):
                target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target);count+=1
    for name in names:
        base=destination/name
        if base.is_dir():
            for path in base.rglob('*'):
                if path.is_file() and not path.name.startswith('.') and path.relative_to(destination) not in expected:path.unlink()
    return count


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['prepare','dispatch','prepare-submit','worker','batch','stop','resume','delete','cancel-batch'])
    parser.add_argument('target');parser.add_argument('extra',nargs='*');parser.add_argument('--name')
    args=parser.parse_args(argv)
    try:
        if args.action in {'prepare','prepare-submit'}:prepare(Path(args.target))
        if args.action in {'dispatch','prepare-submit'}:dispatch(Path(args.target),args.name)
        elif args.action=='worker':worker(args.target)
        elif args.action=='batch':return batch(job_path(args.target),int(args.extra[0]),args.extra[1])
        elif args.action in {'stop','resume','delete'}:action(args.action,args.target)
        elif args.action=='cancel-batch':
            active=read_json(job_path(args.target)/'local-state.json').get('active',{}).get(str(int(args.extra[0])),{})
            if alive(active):os.kill(active['pid'],signal.SIGTERM)
        return 0
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(str(error),file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
