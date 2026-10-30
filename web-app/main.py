#!/usr/bin/env python3
"""Open Slurmy's single-user local web application."""
import sys

if sys.version_info < (3, 9):
    raise SystemExit('Slurmy requires Python 3.9 or newer. Start it with python3 web-app/main.py.')

from slurmy_web.launcher import entrypoint

if __name__ == '__main__':
    raise SystemExit(entrypoint())
