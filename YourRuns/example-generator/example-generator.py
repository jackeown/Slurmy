#!/usr/bin/env python
"""Compatibility shortcut: open the web experiment builder."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from slurmy_web.launcher import entrypoint

if __name__ == '__main__':
    raise SystemExit(entrypoint(['--new', *sys.argv[1:]]))
