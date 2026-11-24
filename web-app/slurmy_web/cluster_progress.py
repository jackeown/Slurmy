"""Read only current pair progress for own running allocations, in one SSH query."""
import csv
import json
from pathlib import Path
import re
import sys


def current_calls(lines, home):
    root=Path(home)/'slurmy/jobs'
    waiting={}
    submissions={}
    for line in lines:
        fields=line.rstrip('\n').split('|')
        if len(fields)<12 or not fields[3].startswith(('RUNNING','COMPLETING')): continue
        identifier,directory,comment=fields[0],Path(fields[10]),fields[11]
        if directory.parent!=root or directory.is_symlink(): continue
        match=re.search(r':batch=(\d+)(?:$|[:;])',comment)
        batch=int(match[1]) if match else None
        parent,_,element=identifier.partition('_')
        offset=re.search(r'^slurmy:v2:.*:batch_offset=(\d+)$',comment)
        if offset and element.isdigit():
            batch=int(offset[1])+int(element)
        if batch is None:
            if directory not in submissions:
                try:
                    with (directory/'submission.csv').open(newline='') as stream: submissions[directory]=list(csv.DictReader(stream))
                except OSError: submissions[directory]=[]
            for item in submissions[directory]:
                if item.get('slurm_job_id')==parent:
                    explicit=item.get('array_task_id', '')
                    if explicit:
                        if explicit != element: continue
                        batch=int(item['batch_id'])
                    else:
                        batch=int(item.get('batch_id') or item.get('offset') or 0)+int(element or 0)
                    break
        if batch is None: continue
        for path in [directory/'progress'/f'batch_{batch:06d}.csv',directory/'progress'/f'batch_{batch:06d}.tsv',directory/'progress'/f'chunk_{batch:06d}.tsv']:
            try:
                with path.open(newline='') as stream: progress=next(csv.reader(stream,delimiter='\t' if path.suffix=='.tsv' else ','))
                if progress[0]!='running' or not progress[1].isdigit(): break
                task=int(progress[1]);waiting.setdefault(directory,[]).append((identifier,task,batch));break
            except (OSError,StopIteration,IndexError,ValueError): continue
    for directory,allocations in waiting.items():
        wanted={task for _,task,_ in allocations};definitions={}
        try:
            with (directory/'manifest.jsonl').open() as stream:
                for index,line in enumerate(stream):
                    if index not in wanted: continue
                    try:
                        definition=json.loads(line);task=int(definition.get('task_id',-1))
                        if task in wanted: definitions[task]=definition
                    except (ValueError,TypeError): continue
            if wanted-definitions.keys():
                with (directory/'manifest.jsonl').open() as stream:
                    for line in stream:
                        try:
                            definition=json.loads(line);task=int(definition.get('task_id',-1))
                            if task in wanted:definitions[task]=definition
                        except (ValueError,TypeError):continue
        except OSError: continue
        for identifier,task,batch in allocations:
            if task in definitions:
                yield identifier,task,{**definitions[task],'batch_id':batch}


if __name__=='__main__':
    for identifier,task,definition in current_calls(sys.stdin,Path.home()):
        print(f'{identifier}|{task}|'+json.dumps(definition,separators=(',',':')))
