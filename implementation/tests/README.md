Run the core and web suites from the repository root in separate Python processes:

```sh
python -m unittest discover -s implementation -p 'test_*.py'
python -m unittest discover -s web-app -p 'test_*.py'
```

Direct discovery also works:

```sh
PYTHONPATH=implementation:web-app python -m unittest discover -s implementation/tests
PYTHONPATH=implementation:web-app python -m unittest discover -s web-app/tests
```

Each test module imports `_bootstrap` before production modules. Keep this import
in new tests so direct discovery can resolve both source directories and shared
test fixtures. The package bootstraps also expose both test directories because
`slurmy_web` may put `implementation` first on the module search path while
unittest is discovering `web-app`. Use separate processes to avoid module state
leaking between suites.

Production fixtures and scripts live one level above each tests directory.
Download wrapper tests import `web-app/download.py`; their `ROOT` is the repository
root (`Path(__file__).resolve().parents[2]`).
