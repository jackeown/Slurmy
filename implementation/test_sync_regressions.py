"""Download correctness regressions; all SSH and transfers are local fakes."""
import csv
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

from remote_export import RESULT_FIELDS

spec = importlib.util.spec_from_file_location('sync_regressions_cli', Path(__file__).with_name('slurmy-sync.py'))
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class SyncRegressions(unittest.TestCase):
    def test_job_id_rejected_before_ssh_or_shell_expansion(self):
        for value in ('$(touch marker)', 'job;true', 'a b', '../job', ''):
            with self.subTest(value=value), patch.object(sync.subprocess, 'run') as run:
                with self.assertRaises(sync.SyncError):
                    sync.resolve_job_id('unused', value, 1)
                run.assert_not_called()

    def test_optional_job_id_is_one_quoted_remote_argument(self):
        for value in (None, 'job-1_2'):
            with patch.object(sync.subprocess, 'run', return_value=SimpleNamespace(
                    returncode=0, stdout=b'job-1_2\n', stderr=b'')) as run:
                self.assertEqual(sync.resolve_job_id('unused', value, 1), 'job-1_2')
                import shlex
                self.assertEqual(shlex.split(run.call_args.args[0][-1]), ['bash', '-s', '--', value or ''])

    def test_saved_status_and_legacy_stderr_status_are_counted(self):
        for kind in ('csv', 'tsv', 'jsonl'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                (root/'results').mkdir()
                (root/'metadata.json').write_text(json.dumps({'task_count': 2}))
                rows = []
                for task in (0, 1):
                    row = dict.fromkeys(RESULT_FIELDS, '')
                    row.update(task_id=str(task), complete='true', status='ok', archive=f'{task}.tar.gz',
                               szs_status='Theorem' if task == 0 else '')
                    rows.append(row)
                    with tarfile.open(root/f'results/{task}.tar.gz', 'w:gz') as archive:
                        # Saved status must take precedence over stale archived text.
                        data = b'% SZS status Satisfiable\n' if task == 0 else b'% SZS status Theorem\n'
                        member = tarfile.TarInfo(f'task_{task:09d}_ab.solver-stderr.log')
                        member.size = len(data)
                        archive.addfile(member, io.BytesIO(data))
                target = root/f'results/result.{kind}'
                if kind == 'jsonl':
                    target.write_text('\n'.join(json.dumps({**r, 'complete': True}) for r in rows))
                else:
                    with target.open('w', newline='') as stream:
                        writer = csv.DictWriter(stream, fieldnames=RESULT_FIELDS,
                                                delimiter='\t' if kind == 'tsv' else ',')
                        writer.writeheader(); writer.writerows(rows)
                summary = sync.build_summary(host='local', job_id='test', destination=root,
                                             changed_paths=0, iteration=1, cache=sync.ArchiveStatusCache())
                self.assertEqual(summary['solved_tasks'], 2)
                self.assertEqual(summary['szs_status_counts'], {'Theorem': 2})

    def test_renamed_download_keeps_old_files_until_success_and_checks_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)/'download'
            args = SimpleNamespace(host='local', output=str(root), ssh_timeout=1, follow=False)
            name = 'Old'
            fail = False
            def names():
                return (f'{name}_results-summary.csv', f'{name}_solver-stdout.tgz', f'{name}_solver-stderr.tgz')
            def prepare(*a, **kw):
                return Mock(stdout=io.StringIO('EXPORT_FILES|'+'|'.join(names())+'\n'), wait=lambda: 0)
            def transfer(command, **kw):
                if fail:
                    return SimpleNamespace(returncode=1, stderr='interrupted')
                stage = Path(command[-1])
                for filename in names():
                    (stage/filename).write_text('complete\ntrue\n' if filename.endswith('.csv') else 'logs')
                return SimpleNamespace(returncode=0, stderr='')
            with patch.object(sync.shutil, 'which', return_value='/fake/tool'), \
                    patch.object(sync.subprocess, 'Popen', side_effect=prepare), \
                    patch.object(sync.subprocess, 'run', side_effect=transfer):
                self.assertEqual(sync.compact_sync_loop(args, 'job'), 0)
                original = {p.name: p.read_bytes() for p in root.iterdir()}
                name = 'New'; fail = True
                with self.assertRaisesRegex(sync.SyncError, 'interrupted'):
                    sync.compact_sync_loop(args, 'job')
                self.assertEqual({p.name:p.read_bytes() for p in root.iterdir()}, original)
                fail = False
                self.assertEqual(sync.compact_sync_loop(args, 'job'), 0)
                self.assertEqual({p.name for p in root.iterdir()}, set(names()))
                with self.assertRaisesRegex(sync.SyncError, 'different host or job'):
                    sync.compact_sync_loop(args, 'other-job')
                # Unrelated files must never be swept away as old exports.
                (root/'keep.txt').write_text('keep')
                with self.assertRaises(sync.SyncError):
                    sync.compact_sync_loop(args, 'job')
                self.assertEqual((root/'keep.txt').read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
