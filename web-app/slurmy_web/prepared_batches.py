"""Inspect the exact local preparation without contacting or submitting to SLURM."""
import csv
from functools import lru_cache
import io
import json
from analyze_run import SCRIPT_NAMES, prepared_scripts_current
from .batches import batch_members, batch_summaries
from .monitor import JobSnapshot, TaskDefinition


@lru_cache(maxsize=2)
def _read(directory, metadata_stamp, manifest_stamp):
    assets=directory/'submit.sh.files'
    metadata=json.loads((assets/'metadata.json').read_text())
    if not isinstance(metadata.get('planned_batch_tasks'),list):
        raise ValueError('This older preparation has no saved batch plan. Run Prepare only again.')
    job=JobSnapshot('prepared',str(directory),metadata_stamp[0]/1e9,metadata=metadata)
    with (assets/'manifest.jsonl').open() as stream:
        for line in stream:
            item=json.loads(line);task=int(item['task_id'])
            job.tasks[task]=TaskDefinition(task,solver=item.get('solver') or item.get('system',''),
                problem=item.get('problem',''),command=item.get('remote_command') or item.get('command',''))
    if any(task not in job.tasks for batch in metadata['planned_batch_tasks'] for task in batch):
        raise ValueError('The prepared batch plan is incomplete. Run Prepare only again.')
    return job


def snapshot(directory):
    try:
        prepared=(directory/'submit.sh').stat()
        metadata=(directory/'submit.sh.files/metadata.json').stat()
        manifest=(directory/'submit.sh.files/manifest.jsonl').stat()
    except FileNotFoundError as exc:
        raise ValueError('No prepared batch plan yet. Choose Prepare only, then open this section.') from exc
    job=_read(directory,(metadata.st_mtime_ns,metadata.st_size,metadata.st_ctime_ns),(manifest.st_mtime_ns,manifest.st_size,manifest.st_ctime_ns))
    inputs=['Makefile','workflow.json','jobpairs.csv','configurations.csv','problem-globs.txt','problem-selection.json','axiom-globs.txt','building.txt','resource_limiter_template.txt',*SCRIPT_NAMES]
    stale=any((directory/name).is_file() and (directory/name).stat().st_mtime_ns>prepared.st_mtime_ns for name in inputs)
    stale = stale or not prepared_scripts_current(directory/'submit.sh.files')
    return job,dict(updated=max(metadata.st_mtime,manifest.st_mtime),stale=stale,refreshing=False)


def summaries(job,page,size):
    value=batch_summaries(job,{},page,size)
    for batch in value['batches']:batch['state']='Prepared'
    return value


def pairs(job,batch,page,size):
    members=batch_members(job)[0]
    if batch not in members:raise ValueError('Batch not found in the prepared plan.')
    ids=members[batch];start=page*size
    return dict(id=batch,total=len(ids),membership_estimated=False,tasks=[
        dict(id=task,position=position,solver=job.tasks[task].solver,problem=job.tasks[task].problem,command=job.tasks[task].command)
        for position,task in enumerate(ids[start:start+size],start+1)])


def csv_rows(job):
    stream=io.StringIO();writer=csv.writer(stream)
    writer.writerow(['batch_id','order_in_batch','pair_id','solver','problem','command'])
    yield stream.getvalue()
    for batch,ids in sorted(batch_members(job)[0].items()):
        for position,task in enumerate(ids,1):
            stream.seek(0);stream.truncate(0);definition=job.tasks[task]
            writer.writerow([batch,position,task,definition.solver,definition.problem,definition.command])
            yield stream.getvalue()
