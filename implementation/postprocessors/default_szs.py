"""Example postprocessor: python3 default_szs.py STDOUT STDERR.

Custom postprocessors can translate SMT sat/unsat into SZS data assignments,
or emit any value, e.g. ``SZS data answer = sat``. Only the standard library
and szs.py are required. In the repository szs.py is in the parent directory;
at runtime it may be copied beside this script in JOB_DIR.
"""
from pathlib import Path
import argparse
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import szs

_OUTPUT = re.compile(r'\bSZS\s+output\s+(?:start|end)\s+([A-Za-z][A-Za-z0-9_-]*)', re.I)
# Vampire and portfolio drivers may prepend elapsed time or process identifiers.
_TERMINATION = re.compile(r'(?:^|\s)%\s+Termination reason:\s+(Time|Memory) limit\s*$', re.I)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stdout')
    parser.add_argument('stderr')
    args = parser.parse_args(argv)
    status = output = ''
    time_limit = memory_limit = False
    try:
        for filename in (args.stdout, args.stderr):
            with open(filename, encoding='utf-8', errors='replace') as stream:
                for line in stream:
                    match = szs.STATUS_RE.search(line)
                    if match:
                        status = szs.canonical(match[1])
                    match = _OUTPUT.search(line)
                    if match:
                        output = szs.data_form(match[1])
                    match = _TERMINATION.search(line)
                    if match:
                        time_limit |= match[1].lower() == 'time'
                        memory_limit |= match[1].lower() == 'memory'
    except OSError as exc:
        parser.exit(1, f'{parser.prog}: {exc}\n')
    if not status:
        status = 'Timeout' if time_limit else 'MemoryOut' if memory_limit else ''
    if status:
        print(f'SZS data status={status}')
    if output:
        print(f'SZS data output={output}')


if __name__ == '__main__':
    main()
