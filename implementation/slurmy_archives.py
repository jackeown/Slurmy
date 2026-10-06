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
MAX_FILES = 100_000
MAX_UNCOMPRESSED = 4 * 1024**3
CACHE = Path(__file__).resolve().parents[1] / 'workflows/my-workflows' / '.problem-archives'


def is_archive(path: Path) -> bool:
    return path.name.lower().endswith(ARCHIVE_SUFFIXES)


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


def _archive_files(path: Path, materialize: bool, suffixes: tuple[str, ...], label: str) -> list[Path]:
    identity = f'{label}:{path.resolve()}:{path.stat().st_size}:{path.stat().st_mtime_ns}'
    destination = CACHE / hashlib.sha256(identity.encode()).hexdigest()[:20]
    names, size = [], 0
    for name, member_size, _ in _members(path, suffixes):
        names.append(name)
        size += member_size
        if len(names) > MAX_FILES or size > MAX_UNCOMPRESSED:
            raise ValueError(f'{label} archive is too large to extract safely: {path}')
    if not names:
        raise ValueError(f'Archive contains no {label.lower()} files ({", ".join(suffixes)}): {path}')
    if len(names) != len(set(names)):
        raise ValueError(f'Archive contains duplicate {label.lower()} paths: {path}')
    if materialize and not (destination / '.complete').is_file():
        CACHE.mkdir(parents=True, exist_ok=True)
        temporary = Path(tempfile.mkdtemp(prefix='.extract-', dir=CACHE))
        try:
            extracted_bytes = 0
            for name, _, opener in _members(path, suffixes):
                target = temporary.joinpath(*name.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                with opener() as source, target.open('wb') as output:
                    while chunk := source.read(1024 * 1024):
                        extracted_bytes += len(chunk)
                        if extracted_bytes > MAX_UNCOMPRESSED:
                            raise ValueError(f'{label} archive is too large to extract safely: {path}')
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


def archive_problems(path: Path, materialize: bool = True) -> list[Path]:
    return _archive_files(path, materialize, PROBLEM_SUFFIXES, 'Problem')


def archive_axioms(path: Path, materialize: bool = True) -> list[Path]:
    return _archive_files(path, materialize, AXIOM_SUFFIXES, 'Axiom')


def expand_axiom_pattern(pattern: str, materialize: bool = True) -> list[tuple[Path, Path]]:
    """Return source files and their paths relative to the job-local Axioms/."""
    files = sorted({Path(os.path.abspath(p)) for p in glob.glob(os.path.expanduser(pattern), recursive=True) if Path(p).is_file()})
    if not files:
        raise ValueError(f'No axiom files matched {pattern}')
    result = []
    for path in files:
        if is_archive(path):
            for source in archive_axioms(path, materialize):
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


def expand_problem_pattern(pattern: str, materialize: bool = True) -> list[Path]:
    files = sorted({Path(os.path.abspath(p)) for p in glob.glob(os.path.expanduser(pattern), recursive=True) if Path(p).is_file()})
    if not files:
        raise ValueError(f'No files matched {pattern}')
    problems = []
    for path in files:
        problems.extend(archive_problems(path, materialize) if is_archive(path) else [path])
    return sorted(set(problems))
