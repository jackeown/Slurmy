"""Local web UI and reusable workflow/job monitoring services."""

import sys
from pathlib import Path

implementation = str(Path(__file__).resolve().parents[2] / 'implementation')
if implementation not in sys.path:
    sys.path.insert(0, implementation)


def create_app():
    from .app import create_app as factory
    return factory()
