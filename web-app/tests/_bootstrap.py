"""Make code and shared test fixtures importable in direct discovery."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
for path in (ROOT / "implementation", ROOT / "web-app", ROOT / "implementation/tests", ROOT / "web-app/tests", Path(__file__).resolve().parent):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))
