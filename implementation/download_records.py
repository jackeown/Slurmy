"""Identify local result snapshots without guessing from human-readable names."""
import hashlib
import json
import os
from pathlib import Path
import tempfile


def record(root, destination, host, job_id):
    root, destination = Path(root).resolve(), Path(destination).resolve()
    # Explicit destinations outside job-results remain under the caller's control.
    if not destination.is_relative_to(root) or destination == root or not destination.is_dir():
        return
    registry = root / '.downloads'
    if registry.is_symlink():
        raise ValueError('Download registry must not be a symlink.')
    registry.mkdir(exist_ok=True)
    key = hashlib.sha256(os.fsencode(destination)).hexdigest()
    fd, temporary = tempfile.mkstemp(dir=registry)
    try:
        with os.fdopen(fd, 'w') as stream:
            json.dump(dict(destination=str(destination), host=host, job_id=job_id), stream)
        os.replace(temporary, registry / (key + '.json'))
    finally:
        Path(temporary).unlink(missing_ok=True)


def snapshots(root, host, job_id):
    """Return only matching, real directories within the local results root."""
    root = Path(root).resolve()
    registry = root / '.downloads'
    found = []
    if registry.is_symlink():
        return found
    for entry in registry.glob('*.json'):
        try:
            if entry.is_symlink():
                continue
            data = json.loads(entry.read_text())
            if not isinstance(data, dict) or (data.get('host'), data.get('job_id')) != (host, job_id):
                continue
            path = Path(data['destination'])
            if path.is_absolute() and path.is_relative_to(root) and path != root and path.is_dir() and path.resolve() == path:
                found.append(path)
        except (OSError, ValueError, KeyError, TypeError):
            continue
    # Older compact/verbose downloads already have exact identity metadata.
    # Recover those too; never guess ownership from a display-name prefix.
    for parent in (root, root/'debug', root/'verbose', root/'starexec'):
        if parent.is_symlink() or not parent.is_dir():
            continue
        for path in parent.iterdir():
            if path.name.startswith('.') or not path.is_dir() or path.is_symlink() or path.resolve()!=path:
                continue
            identity=parent/('.slurmy-export-identity-'+hashlib.sha256(os.fsencode(path)).hexdigest()+'.json')
            for metadata, key in ((identity,'job_id'),(path/'sync-metadata.json','slurmy_job_id')):
                try:
                    if metadata.is_symlink():
                        continue
                    data=json.loads(metadata.read_text())
                    if isinstance(data,dict) and (data.get('host'),data.get(key))==(host,job_id):
                        found.append(path)
                        break
                except (OSError,ValueError):
                    continue
    return sorted(set(found))
