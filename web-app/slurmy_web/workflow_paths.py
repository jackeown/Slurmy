"""Canonical generated filenames, with read-only support for older workflows."""
ALIASES = {'solvers.csv': 'configurations.csv',
           'solver-problem-pairs.csv': 'jobpairs.csv',
           'resource-limiter.txt': 'resource_limiter_template.txt',
           'building.json': 'building.txt'}


def canonical_name(name):
    return next((current for current, old in ALIASES.items() if name == old), name)


def workflow_file(directory, name):
    name = canonical_name(name)
    current = directory / name
    old = directory / ALIASES.get(name, name)
    return old if not current.exists() and old.exists() else current
