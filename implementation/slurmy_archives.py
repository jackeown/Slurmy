"""Expand problem paths and archives into safe, packageable local files."""
from __future__ import annotations
from ssh_transport import connection_options

import glob
import base64
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import tarfile
import tempfile
import zipfile

ARCHIVE_SUFFIXES = ('.zip', '.tar', '.tar.gz', '.tgz', '.tar.bz2', '.tbz2', '.tar.xz', '.txz')
PROBLEM_SUFFIXES = ('.p', '.tptp')
AXIOM_SUFFIXES = ('.ax', '.p', '.tptp')
CACHE = Path(__file__).resolve().parents[1] / 'workflows/my-workflows' / '.problem-archives'
TPTP_ROOT = '/share/TPTP'
TPTP_DEFAULT_VERSION = 'TPTP-v9.2.1'


def tptp_relative(pattern: str) -> str:
    if not isinstance(pattern, str) or not pattern.startswith('tptp://'):
        raise ValueError('Expected a tptp:// path.')
    relative = pattern[len('tptp://'):].strip('/')
    if '..' in PurePosixPath(relative).parts or '\\' in relative or any(ord(c) < 32 for c in relative):
        raise ValueError('Enter a TPTP path without parent-directory components or control characters.')
    # Preserve older saved workflows, which omitted the collection version.
    if relative and not relative.startswith('TPTP-'):
        relative = TPTP_DEFAULT_VERSION + '/' + (relative if relative.startswith('Problems/') else 'Problems/' + relative)
    return relative


def remote_tptp(host: str, relative: str, browse: bool = False, grouped: bool = False):
    script = r'''import base64, glob, json, os, sys
root = sys.argv[1]
relative = '' if sys.argv[2] == '-' else base64.b64decode(sys.argv[2]).decode()
mode = sys.argv[3]
def contained(path):
    return os.path.realpath(path).startswith(os.path.realpath(root) + '/')
if mode == 'browse':
    path = os.path.join(root, relative)
    if not (os.path.realpath(path) == os.path.realpath(root) or contained(path)) or not os.path.isdir(path):
        raise SystemExit('TPTP directory is missing or outside /share/TPTP.')
    entries = []
    for entry in os.scandir(path):
        if entry.name.startswith('.') or not contained(entry.path): continue
        directory = entry.is_dir()
        if directory or (entry.is_file() and entry.name.endswith(('.p', '.tptp'))):
            entries.append(dict(name=entry.name, path='tptp://' + os.path.relpath(entry.path, root), directory=directory))
    entries.sort(key=lambda item: (not item['directory'], item['name'].casefold()))
    current = 'tptp://' + relative
    parent = 'tptp://' + (os.path.dirname(relative) if relative else '')
    print(json.dumps(dict(directory=current, parent=parent, items=entries[:1000], truncated=len(entries)>1000)))
else:
    patterns = json.loads(relative) if mode == 'globs' else [relative]
    matches = {}
    for pattern in patterns:
        files = []
        for path in sorted(glob.iglob(os.path.join(root, pattern), recursive=True)):
            rel = os.path.relpath(path, root).split(os.sep)
            if len(rel) >= 3 and rel[1] == 'Problems' and os.path.isfile(path) and path.endswith(('.p', '.tptp')) and contained(path):
                files.append(path)
        matches[pattern] = files
        if mode != 'globs':
            for path in files: sys.stdout.buffer.write(path.encode() + b'\0')
    if mode == 'globs': print(json.dumps(matches))
'''
    try:
        result = subprocess.run(['ssh', *connection_options(), '-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10', '--', host,
                                 'python3', '-', TPTP_ROOT, base64.b64encode(relative.encode()).decode() or '-',
                                 'browse' if browse else 'globs' if grouped else 'glob'],
                                input=script.encode(), capture_output=True, timeout=120, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValueError(f'Could not access cluster TPTP collection: {error}') from error
    if result.returncode:
        raise ValueError(result.stderr.decode(errors='replace').strip() or 'Could not access cluster TPTP collection.')
    return result.stdout


def browse_cluster_tptp(pattern: str, host: str) -> dict:
    relative = tptp_relative(pattern)
    if any(char in relative for char in '*?['):
        raise ValueError('Browse a directory; add a glob after selecting it.')
    return json.loads(remote_tptp(host, relative, browse=True))


def expand_cluster_tptp(pattern: str, host: str) -> list[str]:
    relative = tptp_relative(pattern)
    if not relative:
        raise ValueError('Select a TPTP version and Problems glob.')
    files = ['tptp:' + path.decode() for path in remote_tptp(host, relative).split(b'\0') if path]
    if not files:
        raise ValueError(f'No TPTP problems matched {pattern} on {host}.')
    return files


def expand_cluster_tptp_many(patterns: list[str], host: str) -> dict[str, list[str]]:
    """Resolve all selected cluster globs in one SSH request, with no stale cache."""
    unique=list(dict.fromkeys(patterns))
    if not unique:return {}
    if len(unique)==1:return {unique[0]:expand_cluster_tptp(unique[0],host)}
    relative=[tptp_relative(pattern) for pattern in unique]
    if any(not item for item in relative):raise ValueError('Select a TPTP version and Problems glob.')
    matches=json.loads(remote_tptp(host,json.dumps(relative),grouped=True))
    result={}
    for pattern,key in zip(unique,relative):
        files=['tptp:'+name for name in matches.get(key,[])]
        if not files:raise ValueError(f'No TPTP problems matched {pattern} on {host}.')
        result[pattern]=files
    return result


def is_archive(path: Path) -> bool:
    return path.name.lower().endswith(ARCHIVE_SUFFIXES)


def _matching_files(pattern: str, progress=None) -> list[Path]:
    files = set()
    if progress:
        progress('scanning', pattern, 0, 0)
    for name in glob.iglob(os.path.expanduser(pattern), recursive=True):
        path = Path(name)
        if path.is_file():
            files.add(Path(os.path.abspath(name)))
            if progress and len(files) % 250 == 0:
                progress('scanning', pattern, len(files), 0)
    if progress:
        progress('indexed', pattern, len(files), 0)
    return sorted(files)


def _safe_name(value: str, suffixes=PROBLEM_SUFFIXES) -> PurePosixPath | None:
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(part in {'', '.', '..'} for part in path.parts):
        return None
    return path if path.name.lower().endswith(suffixes) else None


def _members(path: Path, suffixes=PROBLEM_SUFFIXES):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for member in archive.infolist():
                name = _safe_name(member.filename, suffixes)
                mode = member.external_attr >> 16
                if name and not member.is_dir() and not stat.S_ISLNK(mode):
                    yield name, member.file_size, lambda m=member: archive.open(m)
    elif tarfile.is_tarfile(path):
        with tarfile.open(path, 'r:*') as archive:
            for member in archive:
                name = _safe_name(member.name, suffixes)
                if name and member.isfile():
                    yield name, member.size, lambda m=member: archive.extractfile(m)
    else:
        raise ValueError(f'Not a supported ZIP or tar archive: {path}')


def _archive_files(path: Path, materialize: bool, suffixes: tuple[str, ...], label: str,
                   progress=None) -> list[Path]:
    identity = f'{label}:{path.resolve()}:{path.stat().st_size}:{path.stat().st_mtime_ns}'
    destination = CACHE / hashlib.sha256(identity.encode()).hexdigest()[:20]
    index = destination / '.members.json'
    if (destination / '.complete').is_file() and index.is_file():
        try:
            cached = json.loads(index.read_text(encoding='utf-8'))
            if isinstance(cached, list) and cached and all(isinstance(item, str) and
                    _safe_name(item, suffixes) is not None and (destination / item).is_file()
                    for item in cached):
                return [destination / item for item in cached]
        except (OSError, ValueError, TypeError):
            pass
    names, total_bytes = [], 0
    if progress:
        progress('scanning', str(path), 0, 0)
    for name, member_size, _ in _members(path, suffixes):
        names.append(name)
        total_bytes += member_size
        if progress and len(names) % 250 == 0:
            progress('scanning', str(path), len(names), 0)
    if progress:
        progress('indexed', str(path), len(names), 0)
    if not names:
        raise ValueError(f'Archive contains no {label.lower()} files ({", ".join(suffixes)}): {path}')
    if len(names) != len(set(names)):
        raise ValueError(f'Archive contains duplicate {label.lower()} paths: {path}')
    if materialize and not (destination / '.complete').is_file():
        CACHE.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix='.extract-', dir=CACHE))
        try:
            completed_bytes = 0
            if progress:
                progress('extracting', str(path), completed_bytes, total_bytes)
            for name, _, opener in _members(path, suffixes):
                target = temporary.joinpath(*name.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with opener() as source, target.open('wb') as output:
                    while chunk := source.read(1024 * 1024):
                        output.write(chunk)
                        completed_bytes += len(chunk)
                        if progress:
                            progress('extracting', str(path), completed_bytes, total_bytes)
            (temporary / '.complete').write_text('')
            (temporary / '.members.json').write_text(json.dumps([str(name) for name in names]), encoding='utf-8')
            if destination.exists():
                if not (destination / '.complete').is_file():
                    raise ValueError(f'Incomplete archive cache: {destination}')
                shutil.rmtree(temporary)
            else:
                temporary.rename(destination)
            if progress:
                progress('extracted', str(path), total_bytes, total_bytes)
        except Exception:
            shutil.rmtree(temporary)
            raise
    elif materialize and (destination / '.complete').is_file() and not index.is_file():
        # Older caches predate the member index. Their contents were already extracted.
        index.write_text(json.dumps([str(name) for name in names]), encoding='utf-8')
    return [destination.joinpath(*name.parts) for name in names]


def archive_problems(path: Path, materialize: bool = True, progress=None) -> list[Path]:
    return _archive_files(path, materialize, PROBLEM_SUFFIXES, 'Problem', progress)


def archive_axioms(path: Path, materialize: bool = True, progress=None) -> list[Path]:
    return _archive_files(path, materialize, AXIOM_SUFFIXES, 'Axiom', progress)


def expand_axiom_pattern(pattern: str, materialize: bool = True, progress=None) -> list[tuple[Path, Path]]:
    """Return source files and their paths relative to the job-local Axioms/."""
    files = _matching_files(pattern, progress)
    if not files:
        raise ValueError(f'No axiom files matched {pattern}')
    result = []
    for path in files:
        if is_archive(path):
            for source in archive_axioms(path, materialize, progress):
                # The cache key is the first directory below CACHE.
                member = source.relative_to(CACHE / source.relative_to(CACHE).parts[0])
                parts = member.parts
                relative = Path(*parts[parts.index('Axioms') + 1:]) if 'Axioms' in parts else member
                result.append((source, relative))
        else:
            parts = path.parts
            relative = Path(*parts[parts.index('Axioms') + 1:]) if 'Axioms' in parts else Path(path.name)
            result.append((path, relative))
    return result


def expand_problem_pattern(pattern: str, materialize: bool = True, progress=None) -> list[Path]:
    files = _matching_files(pattern, progress)
    if not files:
        raise ValueError(f'No files matched {pattern}')
    problems = []
    for path in files:
        problems.extend(archive_problems(path, materialize, progress) if is_archive(path) else [path])
    return sorted(set(problems))
