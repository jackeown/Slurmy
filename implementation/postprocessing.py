"""Parse a postprocessor's SZS data protocol without inspecting solver output.

``python3 postprocessing.py LOG`` prints exactly five lines: ASCII JSON data,
known canonical status, known canonical output form, execution classification,
and raw status. Missing or unknown canonical fields are empty lines. The raw
status and JSON preserve custom values; no value is interpreted as a command.
"""
from __future__ import annotations

import argparse
import json
import re
from collections.abc import Iterable

import szs

_DATA = re.compile(r'^\s*%?\s*SZS\s+data\s+([A-Za-z_][A-Za-z0-9_.-]{0,127})\s*=\s*(.*?)\s*$')


def parse_data(text: str | Iterable[str]) -> dict[str, str]:
    """Return literal, case-sensitive assignments; the last assignment wins.

    Surrounding value whitespace is removed. Quotes, commas and equals signs
    are ordinary value characters, with no shell or JSON interpretation.
    """
    result = {}
    for line in text.splitlines() if isinstance(text, str) else text:
        match = _DATA.fullmatch(line.rstrip('\r\n'))
        if match:
            result[match[1]] = match[2]
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', help='postprocessor stdout log')
    args = parser.parse_args(argv)
    try:
        with open(args.log, encoding='utf-8', errors='replace') as stream:
            data = parse_data(stream)
    except OSError as exc:
        parser.exit(1, f'{parser.prog}: {exc}\n')
    status = szs.canonical(data.get('status', ''))
    if not szs.mnemonic(status):
        status = ''
    output = szs.data_form(data.get('output', ''))
    if not szs.data_mnemonic(output):
        output = ''
    print(json.dumps(data, ensure_ascii=True))
    print(status)
    print(output)
    print(szs.execution_status(status) if status else '')
    print(data.get('status', ''))


if __name__ == '__main__':
    main()
