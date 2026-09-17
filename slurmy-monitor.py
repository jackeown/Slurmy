#!/usr/bin/env python
"""Compatibility shortcut: open the local web monitor."""
from slurmy_web.launcher import entrypoint

if __name__ == '__main__':
    raise SystemExit(entrypoint())
