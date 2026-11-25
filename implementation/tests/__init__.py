"""Support package discovery despite the two source trees sharing this name."""
from pathlib import Path
import sys

_ROOT = Path(__file__).resolve().parents[2]
# slurmy_web adds implementation to sys.path during discovery. Include both
# test locations so that importing it cannot shadow the web tests package.
for _base in ("implementation", "web-app"):
    _tests = str(_ROOT / _base / "tests")
    if _tests not in __path__:
        __path__.append(_tests)
for _path in (_ROOT / "implementation", _ROOT / "web-app", _ROOT / "implementation/tests", _ROOT / "web-app/tests", Path(__file__).resolve().parent):
    if str(_path) not in sys.path:
        sys.path.insert(0, str(_path))
