"""Check exported archive contents and preserve exact problem filenames."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
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


spec = importlib.util.spec_from_file_location('slurmy_sync', Path(__file__).parent.parent/'slurmy-sync.py')
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

    def add_postprocessing(self, task_id, data, raw=b'arbitrary,output\nnot JSON\x00\xff'):
        row = self.records[task_id]
        row.update(postprocess_data=json.dumps(data), postprocess_return_code='7',
                   postprocess_error='diagnostic, with\nnewline')
        path = self.root / 'results' / row['archive']
        with tarfile.open(path) as archive:
            contents = [(m.name, archive.extractfile(m).read()) for m in archive if m.isfile()]
        with tarfile.open(path, 'w:gz') as archive:
            for name, content in contents + [(f'task_{task_id}.postprocessor.log', raw),
                                             (f'task_{task_id}.postprocessor-stderr.log', b'diagnostic\n')]:
                member = tarfile.TarInfo(name)
                member.size = len(content)
                archive.addfile(member, io.BytesIO(content))
        csv_path = self.root / 'results' / f'batch_000000_task_{task_id:09d}.csv'
        with csv_path.open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=remote_export.RESULT_FIELDS)
            writer.writeheader()
            writer.writerow(row)
        return raw

    def test_postprocess_csv_union_quoting_status_and_collisions(self):
        self.add_postprocessing(0, {'status': 'custom,\n"status"', 'solver': 'custom solver',
                                  'postprocess.solver': 'literal prefix',
                                  'postprocess.solver.2': 'literal suffix',
                                  'postprocess_data': 'custom raw name',
                                  'execution_status': 'custom execution',
                                  'metric': 'quoted,\n"value"'})
        self.add_postprocessing(1, {'second': 42})
        remote_export.export(self.root)
        with (self.root / 'exports/study_results-summary.csv').open(newline='') as stream:
            reader = csv.DictReader(stream)
            rows = list(reader)
            self.assertEqual(len(reader.fieldnames), len(set(reader.fieldnames)))
        row = rows[0]
        self.assertNotIn('postprocess_data', row)
        self.assertEqual(row['status'], 'custom,\n"status"')
        self.assertEqual(row['execution_status'], 'ok')
        self.assertEqual(row['solver'], 'E_solver + strategy')
        self.assertEqual(row['postprocess.solver'], 'literal prefix')
        self.assertEqual(row['postprocess.solver.2'], 'literal suffix')
        self.assertEqual(row['postprocess.solver.3'], 'custom solver')
        self.assertEqual(row['postprocess.postprocess_data'], 'custom raw name')
        self.assertEqual(row['postprocess.execution_status'], 'custom execution')
        self.assertEqual(row['metric'], 'quoted,\n"value"')
        self.assertEqual(row['postprocess_return_code'], '7')
        self.assertEqual(row['postprocess_error'], 'diagnostic, with\nnewline')
        self.assertEqual(rows[1]['second'], '42')
        self.assertEqual(rows[2]['second'], '')
        self.assertEqual(rows[2]['status'], 'ok')

    def test_raw_status_column_fallback_and_data_precedence(self):
        self.assertEqual(remote_export.RESULT_FIELDS[20], 'postprocess_status')
        self.add_postprocessing(0, {'status': 'sat'})
        for task_id, raw_status in ((0, 'Theorem'), (1, 'sat')):
            row = self.records[task_id]
            row['postprocess_status'] = raw_status
            path = self.root / 'results' / f'batch_000000_task_{task_id:09d}.csv'
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=remote_export.RESULT_FIELDS)
                writer.writeheader()
                writer.writerow(row)
        remote_export.export(self.root)
        with (self.root / 'exports/study_results-summary.csv').open(newline='') as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual([row['status'] for row in rows[:2]], ['sat', 'sat'])
        self.assertEqual([row['execution_status'] for row in rows[:2]], ['ok', 'ok'])

    def test_postprocessing_archive_preserves_arbitrary_streams_compact_and_verbose(self):
        raw = self.add_postprocessing(0, {})
        remote_export.export(self.root)
        sync.consolidate_solver_streams(self.root, self.records)
        for directory in (self.root, self.root / 'exports'):
            with tarfile.open(directory / 'study_postprocessing.tgz') as archive:
                stem = 'study_postprocessing/E_solver + strategy___' + self.problems[0] + '___000000000'
                self.assertEqual(archive.getnames(), ['study_postprocessing', stem + '.log', stem + '.stderr.log'])
                self.assertEqual(archive.extractfile(stem + '.log').read(), raw)
                self.assertEqual(archive.extractfile(stem + '.stderr.log').read(), b'diagnostic\n')
        destination = self.root / 'verbose'
        destination.mkdir()
        sync.publish_verbose_pairs(destination, self.root)
        pair = next((destination / 'solver-problem-pairs').glob('*000000000.tgz'))
        with tarfile.open(pair) as archive:
            member = next(m for m in archive if m.name.endswith('.postprocessor.log'))
            self.assertEqual(archive.extractfile(member).read(), raw)

    def test_legacy_and_invalid_metadata_have_empty_postprocessing_archive(self):
        for value in ('', 'not-json', '[]', 'null'):
            with self.subTest(value=value):
                row = self.records[0]
                row['postprocess_data'] = value
                path = self.root / 'results/batch_000000_task_000000000.csv'
                with path.open('w', newline='') as stream:
                    writer = csv.DictWriter(stream, fieldnames=remote_export.RESULT_FIELDS)
                    writer.writeheader()
                    writer.writerow(row)
                remote_export.export(self.root)
                with tarfile.open(self.root / 'exports/study_postprocessing.tgz') as archive:
                    self.assertEqual(archive.getnames(), ['study_postprocessing'])

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
            'study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz', 'study_postprocessing.tgz'})
        self.assert_archives(directory)
        with (directory / 'study_results-summary.csv').open(newline='') as source:
            rows = list(csv.DictReader(source))
        self.assertEqual([Path(row['problem']).name for row in rows], self.problems)
        self.assertTrue(all(row['has_solver_stdout'] == row['has_solver_stderr'] == 'true' for row in rows))

    def test_verbose_export_matches_compact_archive_naming(self):
        sync.consolidate_solver_streams(self.root, self.records)
        self.assert_archives(self.root)

    def test_starexec_prefers_combined_capture_without_changing_standard_download(self):
        for task_id,row in self.records.items():
            path=self.root/'results'/row['archive']
            # Rebuild fixture archives with a third, independently captured stream.
            with tarfile.open(path, 'w:gz') as archive:
                for suffix,content in [('solver.log',b'pure stdout'),
                                       ('solver-stderr.log',b'pure stderr'),
                                       ('solver-combined.log',b'0.00/0.01\tpure stderr\n0.00/0.02\tpure stdout\n0.00/0.03\tEOF\n')]:
                    member=tarfile.TarInfo(f'task_{task_id:09d}_abcdef.{suffix}')
                    member.size=len(content);archive.addfile(member,io.BytesIO(content))
        remote_export.export(self.root)
        remote_export.export(self.root,combined=True)
        for combined in (False,True):
            directory=self.root/'exports'/('starexec' if combined else '')
            with tarfile.open(directory/'study_solver-stdout.tgz') as archive:
                for member in archive:
                    if member.isfile():
                        content=archive.extractfile(member).read()
                        self.assertEqual(b'pure stderr' in content,combined)
                        self.assertIn(b'pure stdout',content)
            with (directory/'study_results-summary.csv').open(newline='') as stream:
                rows=list(csv.DictReader(stream))
            if combined:
                self.assertTrue(all(row['has_combined_solver_output']=='true' for row in rows))
            else:
                self.assertNotIn('has_combined_solver_output',rows[0])

    def test_verbose_layout_migrates_old_download_and_preserves_raw_references(self):
        originals = {path.name: path.read_bytes() for path in (self.root/'results').iterdir()}
        args = SimpleNamespace(host='test-host', job_id='study', ssh_timeout=10,
                               output=str(self.root), follow=False)
        with patch.object(sync, 'resolve_job_id', return_value='study'), patch.object(sync, 'rsync_job', return_value=0) as transfer:
            self.assertEqual(sync.sync_loop(args), 0)
            transfer.assert_called_with('test-host', 'study', self.root/'diagnostics', 10, mirror=True)
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
        expected = ('study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz', 'study_postprocessing.tgz')
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
                    ['study_results-summary.csv', 'study_solver-stdout.tgz', 'study_solver-stderr.tgz', 'study_postprocessing.tgz']) + '\n'))
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
