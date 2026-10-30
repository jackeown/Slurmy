"""Local submission must work with the Bash shipped by macOS."""
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest


class LocalSubmissionShellTests(unittest.TestCase):
    def test_submission_without_mapfile_with_and_without_connection_sharing(self):
        template = Path(__file__).parent / 'templates' / 'submit.sh'
        for options in ('', '-o\nControlMaster=auto\n-o\nControlPath=/tmp/with space/%C\n'):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                assets = root / 'submit.sh.files'
                (assets / 'builds').mkdir(parents=True)
                (assets / 'workflow_name.sh').write_text('JOB_PREFIX=test\nJOB_DISPLAY_NAME=Test\n')
                (assets / 'submission_settings.sh').write_text(
                    'MAX_OUTSTANDING_DEFAULT=32\nBUILD_COUNT_DEFAULT=0\nLOCAL_PROBLEM_FILES_DEFAULT=0\n')
                for name in ('builds/run.sh', 'remote_prepare.sh', 'remote_finish_upload.sh', 'remote_submit.sh'):
                    (assets / name).write_text(':\n')
                (root / 'submit.sh').write_bytes(template.read_bytes())
                bin_dir = root / 'bin'
                bin_dir.mkdir()
                helpers = {
                    'python': '#!/bin/bash\ncase "$1" in */ssh_transport.py) cat "$OPTIONS_FILE"; exit "${HELPER_EXIT:-0}";; esac\n',
                    'ssh': '#!/bin/bash\nprintf "SSH\\n" >> "$CAPTURE"\nprintf "%s\\n" "$@" >> "$CAPTURE"\ncat >/dev/null\n',
                    'scp': '#!/bin/bash\nprintf "SCP\\n" >> "$CAPTURE"\nprintf "%s\\n" "$@" >> "$CAPTURE"\n',
                }
                for name, body in helpers.items():
                    target = bin_dir / name
                    target.write_text(body)
                    target.chmod(0o755)
                (root / 'options').write_text(options)
                env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ['PATH'],
                           SLURMY_PYTHON=str(bin_dir / 'python'), SLURMY_PARTITION='CPU-amd', CAPTURE=str(root / 'capture'),
                           OPTIONS_FILE=str(root / 'options'))
                env.pop('SLURMY_RESUME_JOB_ID', None)
                # Disabling the newer builtins reproduces the original failure on
                # modern Linux Bash; an actual Bash 3.2 can also run this fixture.
                command = [os.environ.get('SLURMY_TEST_BASH', 'bash'), '-c',
                           'enable -n mapfile readarray 2>/dev/null || :; source "$1"',
                           'local-submit-test', str(root / 'submit.sh')]
                result = subprocess.run(command, env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                capture = (root / 'capture').read_text()
                self.assertEqual(capture.count('SSH\n'), 2)
                self.assertEqual(capture.count('SCP\n'), 1)
                self.assertEqual(capture.count('BatchMode=yes\n'), 3)
                if options:
                    self.assertEqual(capture.count('ControlPath=/tmp/with space/%C\n'), 3)
                self.assertNotIn('BrokenPipe', result.stderr)
                # A failed helper must stop submission before opening SSH.
                (root / 'capture').unlink()
                result = subprocess.run(command, env=dict(env, HELPER_EXIT='23'),
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 23, result.stderr)
                self.assertFalse((root / 'capture').exists())
                # A minimal machine can have only python3, and no rsync.
                (bin_dir / 'python').rename(bin_dir / 'python3')
                for name in ('bash', 'dirname', 'cat', 'mktemp', 'rm', 'date', 'base64', 'tr', 'tar'):
                    (bin_dir / name).symlink_to(shutil.which(name))
                discovery_env = dict(env, PATH=str(bin_dir))
                discovery_env.pop('SLURMY_PYTHON')
                result = subprocess.run(command, env=discovery_env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                # Missing prerequisites fail clearly before any SSH side effects.
                (root / 'capture').unlink()
                (bin_dir / 'tar').unlink()
                result = subprocess.run(command, env=discovery_env, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Missing required tool: tar', result.stderr)
                self.assertFalse((root / 'capture').exists())
