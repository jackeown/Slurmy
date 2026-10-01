"""Expand problem paths and archives into safe, packageable local files."""
from __future__ import annotations

import glob
import hashlib
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import tarfile
import tempfile
import zipfile

ARCHIVE_SUFFIXES = ('.zip', '.tar', '.tar.gz', '.tgz', '.tar.bz2', '.tbz2', '.tar.xz', '.txz')
PROBLEM_SUFFIXES = ('.p', '.tptp')
MAX_FILES = 100_000
MAX_UNCOMPRESSED = 4 * 1024**3
CACHE = Path(__file__).resolve().parents[1] / 'workflows/my-workflows' / '.problem-archives'


def is_archive(path: Path) -> bool:
    return path.name.lower().endswith(ARCHIVE_SUFFIXES)


def _safe_name(value: str) -> PurePosixPath | None:
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(part in {'', '.', '..'} for part in path.parts):
        return None
    return path if path.name.lower().endswith(PROBLEM_SUFFIXES) else None


def _members(path: Path):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for member in archive.infolist():
                name = _safe_name(member.filename)
                mode = member.external_attr >> 16
                if name and not member.is_dir() and not stat.S_ISLNK(mode):
                    yield name, member.file_size, lambda m=member: archive.open(m)
    elif tarfile.is_tarfile(path):
        with tarfile.open(path, 'r:*') as archive:
            for member in archive:
                name = _safe_name(member.name)
                if name and member.isfile():
                    yield name, member.size, lambda m=member: archive.extractfile(m)
    else:
        raise ValueError(f'Not a supported ZIP or tar archive: {path}')


def archive_problems(path: Path, materialize: bool = True) -> list[Path]:
    identity = f'{path.resolve()}:{path.stat().st_size}:{path.stat().st_mtime_ns}'
    destination = CACHE / hashlib.sha256(identity.encode()).hexdigest()[:20]
    names, size = [], 0
    for name, member_size, _ in _members(path):
        names.append(name)
        size += member_size
        if len(names) > MAX_FILES or size > MAX_UNCOMPRESSED:
            raise ValueError(f'Problem archive is too large to extract safely: {path}')
    if not names:
        raise ValueError(f'Archive contains no .p or .tptp problem files: {path}')
    if len(names) != len(set(names)):
        raise ValueError(f'Archive contains duplicate problem paths: {path}')
    if materialize and not (destination / '.complete').is_file():
        CACHE.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix='.extract-', dir=CACHE))
        try:
            extracted_bytes = 0
            for name, _, opener in _members(path):
                target = temporary.joinpath(*name.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with opener() as source, target.open('wb') as output:
                    while chunk := source.read(1024 * 1024):
                        extracted_bytes += len(chunk)
                        if extracted_bytes > MAX_UNCOMPRESSED:
                            raise ValueError(f'Problem archive is too large to extract safely: {path}')
                        output.write(chunk)
            (temporary / '.complete').write_text('')
            if destination.exists():
                if not (destination / '.complete').is_file():
                    raise ValueError(f'Incomplete archive cache: {destination}')
                shutil.rmtree(temporary)
            else:
                temporary.rename(destination)
        except Exception:
            shutil.rmtree(temporary)
            raise
    return [destination.joinpath(*name.parts) for name in names]


def expand_problem_pattern(pattern: str, materialize: bool = True) -> list[Path]:
    files = sorted({Path(os.path.abspath(p)) for p in glob.glob(os.path.expanduser(pattern), recursive=True) if Path(p).is_file()})
    if not files:
        raise ValueError(f'No files matched {pattern}')
    problems = []
    for path in files:
        problems.extend(archive_problems(path, materialize) if is_archive(path) else [path])
    return sorted(set(problems))
