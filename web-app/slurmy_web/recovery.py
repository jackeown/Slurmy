"""Inspect and recover locally archived workflows without following resource links."""
from datetime import datetime
from pathlib import Path
import os
import re
import shutil

from . import workflows


def entry(archive_id):
    if not isinstance(archive_id,str) or not re.fullmatch(r'[^/\\\\]+-[0-9]{15,20}',archive_id):
        raise ValueError('Invalid recovery archive entry.')
    root=workflows.DELETED
    if root.is_symlink():raise ValueError('Recovery archive must not be a symlink.')
    path=root/archive_id
    if path.is_symlink() or not path.is_dir():raise ValueError('Archived workflow no longer exists.')
    name,stamp=archive_id.rsplit('-',1)
    if name in ('.','..'):raise ValueError('Invalid archived workflow name.')
    return path,dict(id=archive_id,name=name.removeprefix('example-'),directory_name=name,
                     archived=datetime.fromtimestamp(int(stamp)/1e9).astimezone().strftime('%Y-%m-%d %H:%M:%S'))


def entries():
    root=workflows.DELETED
    if root.is_symlink():raise ValueError('Recovery archive must not be a symlink.')
    if not root.is_dir():return []
    items=[]
    for path in root.iterdir():
        try:_,item=entry(path.name)
        except (ValueError,FileNotFoundError):continue
        items.append(item)
    return sorted(items,key=lambda item:int(item['id'].rsplit('-',1)[1]),reverse=True)


def inspect(archive_id):
    root,item=entry(archive_id)
    files=[]
    pending=[root]
    while pending and len(files)<300:
        folder=pending.pop()
        with os.scandir(folder) as children:
            for child in children:
                if len(files)>=300:break
                path=Path(child.path)
                linked=child.is_symlink()
                directory=child.is_dir(follow_symlinks=False)
                files.append(dict(path=path.relative_to(root).as_posix(),kind='link' if linked else 'directory' if directory else 'file',
                                  bytes=None if directory or linked else child.stat(follow_symlinks=False).st_size))
                if directory:pending.append(path)
    return dict(**item,files=sorted(files,key=lambda value:value['path']),truncated=bool(pending) or len(files)>=300)


def restore(archive_id):
    source,item=entry(archive_id)
    destination=workflows.USER_RUNS/item['directory_name']
    if destination.exists() or destination.is_symlink():
        raise ValueError('A workflow with this name already exists. Rename or archive it before restoring this copy.')
    source.rename(destination)
    return destination


def remove(archive_id):
    source,_=entry(archive_id)
    shutil.rmtree(source)
