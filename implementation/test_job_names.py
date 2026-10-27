"""Automatic names are short, numbered only on collision, and stable on retry."""
import base64
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from job_names import unused_job_name


class JobNameTests(unittest.TestCase):
    def test_suffixes_and_long_names(self):
        self.assertEqual(unused_job_name('workflow',set()),'workflow')
        self.assertEqual(unused_job_name('workflow',{'workflow','workflow (1)'}),'workflow (2)')
        base='a'*100
        name=unused_job_name(base,{base})
        self.assertLessEqual(len(name),100)
        self.assertTrue(name.endswith(' (1)'))

    def test_metadata_names_and_ids_survive_retries_and_custom_names(self):
        script=(Path(__file__).parent/'templates/remote_submit.sh').read_text()
        code=re.search(r'python3 - "\$\{5:-\}" "\$\{8:-0\}" <<\'PY\'\n(.*?)\nPY',script,re.S).group(1)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def assign(number,name='workflow',auto='1'):
                folder=root/f'workflow_1793101588_{number}'
                folder.mkdir(exist_ok=True)
                meta=folder/'metadata.json'
                if not meta.exists():meta.write_text(json.dumps(dict(workflow_directory='/workflows/workflow')))
                subprocess.run([sys.executable,'-',base64.b64encode(name.encode()).decode(),auto],input=code,text=True,
                    cwd=folder,env={**os.environ,'PYTHONPATH':str(Path(__file__).parent.resolve())},check=True,capture_output=True)
                data=json.loads(meta.read_text());self.assertEqual(data['job_id'],folder.name)
                return data['job_name']
            self.assertEqual(assign(1),'workflow')
            self.assertEqual(assign(2),'workflow (1)')
            self.assertEqual(assign(2),'workflow (1)')
            self.assertEqual(assign(3),'workflow (2)')
            self.assertEqual(assign(4,'Custom display name','0'),'Custom display name')
