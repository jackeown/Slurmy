"""Expose local executions through the same job/result views as cluster jobs."""
import csv
import json
from pathlib import Path
import time
from datetime import datetime
from local_backend import jobs, job_path, read_json, alive, root
from .monitor import (JobSnapshot,ClusterSnapshot,TaskDefinition,BatchProgress,SlurmRecord,
                      parse_csv_results,summarize_results)


def collect(detail=None):
    snapshots=[]
    for item in jobs():
        if detail and item['id']!=detail:continue
        path=job_path(item['id']);metadata=read_json(path/'metadata.json');state=read_json(path/'local-state.json')
        live=alive(read_json(path/'controller.json')) and state.get('state') not in {'done','stopped','incomplete'}
        metadata['local_state']=state.get('state','')
        job=JobSnapshot(item['id'],str(path),float(item['submitted']),metadata=metadata,queue_available=True)
        job.controller_state='active' if live else 'inactive'
        job.submission_state='stopped' if state.get('state')=='stopped' else 'submitting' if live else 'failed' if state.get('state') in {'starting','building','running','pending','incomplete'} else 'complete'
        log=path/'logs/submission-controller.log'
        job.controller_log=log.read_text(errors='replace')[-16000:] if log.is_file() else ''
        for batch_id,task_ids in enumerate(metadata.get('planned_batch_tasks',[])):
            for task in task_ids:job.task_batches[task]=batch_id
        job.submitted_batches=set(range(job.batch_count));job.submission_count=job.batch_count
        if detail:
            for line in (path/'manifest.jsonl').read_text().splitlines():
                row=json.loads(line);task=row['task_id']
                job.tasks[task]=TaskDefinition(task_id=task,task_key=row.get('task_key',''),solver=row.get('solver',''),problem=row.get('problem',''),command=row.get('command',''),limiter_command=row.get('limiter_command',''),solver_root=row.get('solver_root',''))
        for result in (path/'results').glob('batch_*_task_*.csv'):
            job.results.update(parse_csv_results(result.read_text()))
        summarize_results(job,job.results)
        if detail:
            for progress in (path/'progress').glob('task_*.csv'):
                with progress.open() as stream:values=next(csv.reader(stream),[])
                if len(values)>=5:
                    task=int(values[1]);job.call_progress[task]=BatchProgress(int(values[4]),values[0],task,float(values[2]),float(values[3]))
        if live:
            pid=str(read_json(path/'controller.json').get('pid',''))
            for bid,active in state.get('active',{}).items():
                if not bid.isdecimal():continue
                job.slurm.append(SlurmRecord(f'{pid}_{bid}',pid,bid,'RUNNING','','',job.job_name,str(path),metadata.get('local_node','this computer'),'queue',start=datetime.fromtimestamp(active['started']).astimezone().isoformat(),submitted=datetime.fromtimestamp(job.created_epoch).astimezone().isoformat()))
            if not job.slurm:
                job.slurm.append(SlurmRecord(pid,pid,'','PENDING','','',job.job_name,str(path),'this computer','queue'))
        for bid,attempt in state.get('history',{}).items():
            pid=str(attempt.get('parent',''))
            job.slurm.append(SlurmRecord(f'{pid}_{bid}',pid,bid,'COMPLETED' if attempt.get('return_code')==0 else 'CANCELLED','','',job.job_name,str(path),'this computer','accounting',
                submitted=datetime.fromtimestamp(job.created_epoch).astimezone().isoformat(),start=datetime.fromtimestamp(attempt['started']).astimezone().isoformat(),end=datetime.fromtimestamp(attempt['ended']).astimezone().isoformat()))
        snapshots.append(job)
    return ClusterSnapshot('local',str(root()),detail or '',time.time(),snapshots)


def activity(identifiers):
    rows=[]
    for identifier in identifiers:
        path=job_path(identifier);live=alive(read_json(path/'controller.json'))
        rows.append(dict(job_id=identifier,exists=path.is_dir(),active=live,stopped=read_json(path/'local-state.json').get('state')=='stopped',controller_state='active' if live else 'inactive'))
    return dict(queue_available=True,jobs=rows)


def batch_log(identifier,display_id):
    batch=display_id.rsplit('_',1)[-1]
    if not batch.isdecimal():raise ValueError('Invalid local batch ID.')
    path=job_path(identifier)/f'logs/local-batch-{batch}.log'
    return path.read_text(errors='replace')[-100000:] if path.is_file() else ''


def problem_text(identifier,problem):
    path=job_path(identifier)/'rootfs'/problem.lstrip('/')
    base=(job_path(identifier)/'rootfs').resolve()
    if not path.resolve().is_relative_to(base):raise ValueError('Invalid problem path.')
    with path.open('rb') as stream:data=stream.read(1024*1024+1)
    return data[:1024*1024].decode(errors='replace'),len(data)>1024*1024
