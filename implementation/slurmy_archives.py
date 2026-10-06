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
AXIOM_SUFFIXES = ('.ax', '.p', '.tptp')
CACHE = Path(__file__).resolve().parents[1] / 'workflows/my-workflows' / '.problem-archives'


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
