#!/usr/bin/env python
"""Open Slurmy's single-user local web application."""
from slurmy_web.launcher import entrypoint

if __name__ == '__main__':
    raise SystemExit(entrypoint())
