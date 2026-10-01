#!/usr/bin/env python
"""Compatibility shortcut: open the local web monitor."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'web-app'))
from slurmy_web.launcher import entrypoint

if __name__ == '__main__':
    raise SystemExit(entrypoint())
