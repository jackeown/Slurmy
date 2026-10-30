"""Preparation expands phony problem sources once and can emit compact bundles."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import tarfile
from unittest.mock import patch

from slurmy import REPO,generate
import test_batching


class FastPreparationTests(unittest.TestCase):
    def test_compact_bundles_match_readable_scripts(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            assets=test_batching.SequentialBatchTests().prepare(root,[(False,False)]*6,4)
            reference={}
            for path in (assets/'call-bundles').glob('*.tar.gz'):
                with tarfile.open(path) as archive:
                    reference[path.name]={m.name:archive.extractfile(m).read() for m in archive if m.isfile()}
            import shutil
            shutil.rmtree(assets);(root/'submit.sh').unlink()
            generate(root/'jobpairs.csv',root/'building.txt',root/'limiter.txt',batch_size=4,materialize_calls=False)
            self.assertFalse((assets/'calls').exists())
            for path in (assets/'call-bundles').glob('*.tar.gz'):
                with tarfile.open(path) as archive:
                    self.assertEqual({m.name:archive.extractfile(m).read() for m in archive if m.isfile()},reference[path.name])
            self.assertIn('LOCAL_PROBLEM_FILES_DEFAULT=1',(assets/'submission_settings.sh').read_text())

    def test_make_prepare_expands_phony_problem_sources_once(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);tools=root/'tools';tools.mkdir()
            python=tools/'python3';python.write_text('''#!/bin/sh
case "$1" in
 *check_threads.py) echo validate >> stages;;
 *slurmy.py) echo compile >> stages; mkdir -p submit.sh.files; touch submit.sh;;
 *) exit 3;;
esac
''');python.chmod(0o755)
            for name in ('building.txt','resource_limiter_template.txt'):(root/name).touch()
            (root/'Makefile').write_text(f'''REPO_ROOT := {REPO}
include {REPO}/implementation/templates/workflow.mk
.PHONY: refresh-problems
jobpairs.csv: refresh-problems
\t@echo expand >> expansions
\t@touch jobpairs.csv
''')
            subprocess.run(['make','prepare'],cwd=root,env={**os.environ,'PATH':str(tools)+os.pathsep+os.environ['PATH']},check=True,capture_output=True)
            self.assertEqual((root/'expansions').read_text().splitlines(),['expand'])
            self.assertEqual((root/'stages').read_text().splitlines(),['validate','compile'])
