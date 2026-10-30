"""Check exported archive contents and preserve exact problem filenames."""
import csv
import importlib.util
import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import remote_export


spec = importlib.util.spec_from_file_location('slurmy_sync', Path(__file__).with_name('slurmy-sync.py'))
sync = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sync)


class ExportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / 'results').mkdir()
        self.problems = ['SET002+3.p', 'SET002^3.p', 'SET_002 ü.p', 'SET' + '0' * 90 + '+3.p']
        definitions, self.records = [], {}
        (self.root / 'metadata.json').write_text(json.dumps({
            'job_name': 'study', 'task_count': len(self.problems)}))
        for task_id, problem in enumerate(self.problems):
            definitions.append({'task_id': task_id, 'solver': 'E_solver + strategy',
                                'problem': '/problems/SET/' + problem})
            archive = f'batch_0_task_{task_id}.tar.gz'
            with tarfile.open(self.root / 'results' / archive, 'w:gz') as output:
                for suffix, content in (('solver.log', b'solver stdout'),
                                        ('solver-stderr.log', b'solver stderr')):
                    member = tarfile.TarInfo(f'task_{task_id:09d}_abcdef.{suffix}')
                    member.size = len(content)
                    output.addfile(member, io.BytesIO(content))
            row = {'task_id': str(task_id), 'complete': 'true', 'status': 'ok', 'archive': archive}
            self.records[task_id] = row
            with (self.root / 'results' / f'batch_000000_task_{task_id:09d}.csv').open('w', newline='') as output:
                writer = csv.DictWriter(output, fieldnames=remote_export.RESULT_FIELDS)
                writer.writeheader()
                writer.writerow(row)
        (self.root / 'manifest.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in definitions))

    def assert_archives(self, directory):
        for stream, suffix in (('stdout', '.log'), ('stderr', '.stderr.log')):
            archive_root = f'study_solver-{stream}'
            with tarfile.open(directory / f'{archive_root}.tgz') as archive:
                members = [member for member in archive if member.isfile()]
                self.assertEqual([member.name for member in members], [
                    f'{archive_root}/E_solver + strategy___{problem}___{task_id:09d}{suffix}'
                    for task_id, problem in enumerate(self.problems)])
                for member in members:
                    self.assertEqual(archive.extractfile(member).read(), f'solver {stream}'.encode())

    def test_compact_export_names_and_contents(self):
        remote_export.export(self.root)
        directory = self.root / 'exports'
        self.assertEqual({path.name for path in directory.iterdir() if not path.name.startswith('.')}, {
            'study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz'})
        self.assert_archives(directory)
        with (directory / 'study_results-summary.csv').open(newline='') as source:
            rows = list(csv.DictReader(source))
        self.assertEqual([Path(row['problem']).name for row in rows], self.problems)
        self.assertTrue(all(row['has_solver_stdout'] == row['has_solver_stderr'] == 'true' for row in rows))

    def test_verbose_export_matches_compact_archive_naming(self):
        sync.consolidate_solver_streams(self.root, self.records)
        self.assert_archives(self.root)

    def test_verbose_layout_migrates_old_download_and_preserves_raw_references(self):
        originals = {path.name: path.read_bytes() for path in (self.root/'results').iterdir()}
        args = SimpleNamespace(host='test-host', job_id='study', ssh_timeout=10,
                               output=str(self.root), follow=False)
        with patch.object(sync, 'resolve_job_id', return_value='study'), patch.object(sync, 'rsync_job', return_value=0) as transfer:
            self.assertEqual(sync.sync_loop(args), 0)
            transfer.assert_called_with('test-host', 'study', self.root/'diagnostics', 10)
            self.assertFalse((self.root/'results').exists())
            self.assert_archives(self.root)
            for name, body in originals.items():
                self.assertEqual((self.root/'diagnostics/results'/name).read_bytes(), body)
            pairs = self.root/'solver-problem-pairs'
            expected = []
            for task_id, problem in enumerate(self.problems):
                stem = f'E_solver + strategy___{problem}___{task_id:09d}'
                path = pairs/(stem+'.tgz')
                expected.append(path.name)
                with tarfile.open(path) as archive:
                    self.assertEqual(archive.getnames(), [stem, stem+'/'+stem+'.log', stem+'/'+stem+'.stderr.log'])
                    self.assertEqual(archive.extractfile(stem+'/'+stem+'.log').read(), b'solver stdout')
                    self.assertEqual(archive.extractfile(stem+'/'+stem+'.stderr.log').read(), b'solver stderr')
            self.assertEqual(sorted(p.name for p in pairs.glob('*.tgz')), sorted(expected))
            before = {p.name: p.stat().st_mtime_ns for p in pairs.glob('*.tgz')}
            self.assertEqual(sync.sync_loop(args), 0)
            self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in pairs.glob('*.tgz')})
            summary = json.loads((self.root/'sync-metadata.json').read_text())
            self.assertEqual(summary['completed_tasks'], len(self.problems))
            self.assertEqual(summary['local_directory'], str(self.root))
            self.assertTrue((self.root/'CONTENTS.txt').is_file())

    def test_verbose_pair_republish_replaces_latest_but_keeps_raw_history(self):
        destination = self.root/'readable'
        destination.mkdir()
        sync.publish_verbose_pairs(destination, self.root)
        record = self.records[0]
        old = self.root/'results'/record['archive']
        new = self.root/'results'/'batch_retry.tar.gz'
        shutil.copyfile(old, new)
        csv_path = self.root/'results/batch_000000_task_000000000.csv'
        csv_path.write_text(csv_path.read_text().replace(old.name, new.name))
        unrelated = destination/'solver-problem-pairs/notes.txt'
        unrelated.write_text('keep me')
        sync.publish_verbose_pairs(destination, self.root)
        self.assertTrue(old.exists())
        self.assertTrue(new.exists())
        self.assertEqual(len(list((destination/'solver-problem-pairs').glob('*.tgz'))), len(self.problems))
        self.assertEqual(unrelated.read_text(), 'keep me')

    def test_plus_and_caret_are_distinct_even_for_same_task_id(self):
        self.assertNotEqual(
            remote_export.stream_name({'problem': 'SET002+3.p'}, 0, False),
            remote_export.stream_name({'problem': 'SET002^3.p'}, 0, False))

    def test_old_export_names_do_not_suppress_regeneration(self):
        directory = self.root / 'exports'
        directory.mkdir()
        for name in ('study.csv', 'study_logs.tgz', 'study_errors.tgz'):
            (directory / name).write_bytes(b'old export')
        (directory / '.signature').write_text('old format')
        remote_export.export(self.root)
        self.assert_archives(directory)
        self.assertTrue((directory / 'study_results-summary.csv').is_file())

    def test_sync_replaces_old_download_names_only_after_success(self):
        remote_export.export(self.root)
        source = self.root / 'exports'
        expected = ('study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz')
        for fail in (True, False):
            with self.subTest(transfer_fails=fail):
                destination = self.root / ('failed-download' if fail else 'download')
                destination.mkdir()
                old_names = ('study.csv', 'study_logs.tgz', 'study_errors.tgz')
                for name in old_names:
                    (destination / name).write_bytes(b'previous download')
                process = Mock(stdout=io.StringIO('EXPORT_FILES|' + '|'.join(expected) + '\n'))
                process.wait.return_value = 0

                def transfer(*args, **kwargs):
                    if not fail:
                        for name in expected:
                            shutil.copyfile(source / name, Path(args[0][-1]) / name)
                    return SimpleNamespace(returncode=int(fail), stderr='transfer failed' if fail else '')

                args = SimpleNamespace(host='test-host', job_id='study', ssh_timeout=10,
                                       output=str(destination), follow=False)
                with patch.object(sync, 'resolve_job_id', return_value='study'), \
                     patch.object(sync.shutil, 'which', return_value='/test/tool'), \
                     patch.object(sync.subprocess, 'Popen') as popen, \
                     patch.object(sync.subprocess, 'run', side_effect=transfer):
                    popen.return_value = process
                    if fail:
                        with self.assertRaises(sync.SyncError):
                            sync.compact_sync_loop(args)
                        self.assertTrue(all((destination / name).read_bytes() == b'previous download'
                                            for name in old_names))
                    else:
                        self.assertEqual(sync.compact_sync_loop(args), 0)
                        self.assertEqual({path.name for path in destination.iterdir()}, set(expected))
                        self.assert_archives(destination)

    def test_scp_fallback_stages_download_and_preserves_existing_files_on_failure(self):
        remote_export.export(self.root)
        source = self.root / 'exports'
        expected = tuple(sorted(p.name for p in source.iterdir() if not p.name.startswith('.')))
        for fail in (True, False):
            with self.subTest(fail=fail):
                destination = self.root / ('scp-fail' if fail else 'scp-ok')
                destination.mkdir()
                for name in expected:
                    (destination / name).write_bytes(b'previous export')
                process = Mock(stdout=io.StringIO('EXPORT_FILES|' + '|'.join(
                    ['study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz']) + '\n'))
                process.wait.return_value = 0

                def transfer(command, **kwargs):
                    self.assertEqual(command[0], 'scp')
                    stage = Path(command[-1])
                    self.assertNotEqual(stage, destination)
                    for name in expected:
                        shutil.copyfile(source / name, stage / name)
                    return SimpleNamespace(returncode=int(fail), stderr='interrupted' if fail else '')

                args = SimpleNamespace(host='test-host', job_id='study', ssh_timeout=10,
                                       output=str(destination), follow=False)
                with patch.object(sync, 'resolve_job_id', return_value='study'), \
                     patch.object(sync.shutil, 'which', side_effect=lambda tool: None if tool == 'rsync' else '/test/' + tool), \
                     patch.object(sync.subprocess, 'Popen', return_value=process), \
                     patch.object(sync.subprocess, 'run', side_effect=transfer):
                    if fail:
                        with self.assertRaisesRegex(sync.SyncError, 'interrupted'):
                            sync.compact_sync_loop(args)
                        self.assertTrue(all((destination / name).read_bytes() == b'previous export' for name in expected))
                    else:
                        self.assertEqual(sync.compact_sync_loop(args), 0)
                        self.assert_archives(destination)
                self.assertFalse(list(destination.parent.glob('.slurmy-download-*')))


if __name__ == '__main__':
    unittest.main()
