"""Submission bundles avoid per-pair shared-filesystem writes and SSH handshakes."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import test_batching
from ssh_transport import connection_options
from slurmy_archives import expand_cluster_tptp_many


class SubmissionLatencyTests(unittest.TestCase):
    def test_scripts_are_bundled_per_batch_and_not_unpacked_during_upload(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            assets=test_batching.SequentialBatchTests().prepare(root,[(False,False)]*201,100)
            bundles=sorted((assets/'call-bundles').glob('*.tar.gz'))
            self.assertEqual(len(bundles),3)
            for path,count in zip(bundles,[100,100,1]):
                with tarfile.open(path) as archive:
                    self.assertEqual(len([n for n in archive.getnames() if n.endswith('/config.sh')]),count)
                    self.assertTrue(all(not n.startswith('/') and '..' not in Path(n).parts for n in archive.getnames()))
            packed=root/'job-files.tar.gz'
            subprocess.run([sys.executable,str(Path(__file__).with_name('package_inputs.py')),
                            '--label','test','--output',str(packed),'--base',str(assets)],
                           check=True,capture_output=True)
            with tarfile.open(packed) as archive:
                names=archive.getnames()
                self.assertFalse(any(n=='./calls' or n.startswith('./calls/') for n in names))
                self.assertEqual(sum(n.endswith('.tar.gz') for n in names),3)
                extracted=root/'remote'
                archive.extractall(extracted)
            with tarfile.open(extracted/'call-bundles/batch_000002.tar.gz') as archive:
                archive.extractall(root/'scratch')
            self.assertEqual((root/'scratch/200/config.sh').read_bytes(),(assets/'calls/200/config.sh').read_bytes())

    def test_all_cluster_globs_are_resolved_in_one_request(self):
        patterns=['tptp://TPTP-v9.2.1/Problems/AGT/*.p','tptp://TPTP-v9.2.1/Problems/SET/*.p']
        response={p.removeprefix('tptp://'):['/share/TPTP/'+p.removeprefix('tptp://').replace('*.p','test.p')] for p in patterns}
        with patch('slurmy_archives.remote_tptp',return_value=json.dumps(response).encode()) as remote:
            matches=expand_cluster_tptp_many(patterns+patterns[:1],'datalab')
        self.assertEqual(remote.call_count,1)
        self.assertTrue(remote.call_args.kwargs['grouped'])
        self.assertEqual(set(matches),set(patterns))
        with patch('slurmy_archives.remote_tptp',return_value=b'{}'):
            with self.assertRaisesRegex(ValueError,'No TPTP problems'):
                expand_cluster_tptp_many(patterns,'datalab')

    def test_connection_sockets_are_private_and_unsafe_directories_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder,patch('ssh_transport.tempfile.gettempdir',return_value=folder):
            options=connection_options()
            self.assertIn('ControlMaster=auto',options)
            directory=Path(folder)/f'slurmy-ssh-{os.getuid()}'
            self.assertEqual(directory.stat().st_mode & 0o777,0o700)
            directory.chmod(0o755)
            self.assertEqual(connection_options(),[])
            directory.rmdir()
            directory.symlink_to(Path(folder))
            self.assertEqual(connection_options(),[])
