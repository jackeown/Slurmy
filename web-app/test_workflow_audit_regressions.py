"""Local regressions for workflow copying, transactional rename and portability."""
import json
from datetime import datetime
from pathlib import Path
import shutil
import unittest
from unittest.mock import patch

from slurmy import read_builds
from slurmy_web import workflows, portable
from slurmy_web.app import create_app
import test_workflow_files as fixtures


class WorkflowAuditTests(unittest.TestCase):
    setUp = fixtures.WorkflowFileTests.setUp

    def test_creation_date_matches_card_after_edit(self):
        data = workflows.decisions(self.directory)
        data['_created_at'] = 1600000000
        workflows.save(data, self.directory)
        # The browser submits editable fields, not internal creation metadata.
        data.pop('_created_at')
        workflows.rename_and_save(data, self.directory)
        with patch.object(workflows, 'migrate_user_runs'):
            client = create_app().test_client()
        expected = datetime.fromtimestamp(1600000000).astimezone().strftime('%Y-%m-%d %H:%M')
        for route in ['/workflows', '/workflow']:
            response = client.get(route, query_string={'directory': str(self.directory)})
            self.assertEqual(response.status_code, 200)
            self.assertIn(expected, response.text)
        data['name'] = 'renamed'
        renamed, _ = workflows.rename_and_save(data, self.directory)
        self.assertEqual(workflows.decisions(renamed)['_created_at'], 1600000000)

    def snapshot(self, directory):
        return {str(p.relative_to(directory)): (p.read_bytes(), p.stat().st_mode)
                for p in directory.rglob('*') if p.is_file()}

    def test_duplicate_keeps_workflow_local_source(self):
        source = self.directory/'source'; source.mkdir()
        (source/'main.c').write_text('source input')
        recipe = self.directory/'build.sh'; recipe.write_text('true\n')
        data = workflows.decisions(self.directory)
        data['resources'] = [dict(root=str(self.runtime), source=str(source), mode='script',
                                  script=str(recipe), artifact='solver', build_policy='changed')]
        workflows.save(data, self.directory)
        destination = workflows.duplicate(self.directory, 'copy')
        self.assertEqual((destination/'source/main.c').read_text(), 'source input')
        builds = read_builds(destination/'building.txt')
        self.assertEqual(builds[0].source, destination/'source')
        self.assertEqual(builds[0].build_policy, 'changed')
        shutil.rmtree(self.directory)
        self.assertTrue(read_builds(destination/'building.txt')[0].recipe.is_file())

    def test_duplicate_existing_build_definition_preserves_all_fields(self):
        recipe = self.directory/'build.sh'; recipe.write_text('true\n')
        definitions = [dict(root=str(self.runtime), source=str(self.runtime), recipe=str(recipe),
                            artifact='solver', build_policy='changed')]
        (self.directory/'building.txt').write_text(json.dumps(definitions))
        data = workflows.decisions(self.directory)
        data.update(building_mode='existing', building_file=str(self.directory/'building.txt'), resources=[])
        (self.directory/'.slurmy-workflow.json').write_text(json.dumps(data))
        destination = workflows.duplicate(self.directory, 'copy')
        build, = read_builds(destination/'building.txt')
        self.assertEqual(build.recipe, destination/'build.sh')
        self.assertEqual(build.source, self.runtime)
        self.assertEqual(build.artifact, 'solver')
        self.assertEqual(build.build_policy, 'changed')
        manifest = workflows.decisions(destination)
        self.assertEqual(manifest['building_file'], str(destination/'building.txt'))
        self.assertEqual(manifest['resources'][0]['script'], str(build.recipe))

    def test_editor_rename_preserves_sibling_paths_and_relocates_own(self):
        sibling = self.users/'test-other'; sibling.mkdir()
        problem = sibling/'p.p'; problem.write_text('cnf(a,axiom,p).')
        own = self.directory/'own.p'; own.write_text('cnf(b,axiom,q).')
        data = workflows.decisions(self.directory)
        data.update(name='renamed', globs=[str(problem), str(own)])
        destination, count = workflows.rename_and_save(data, self.directory)
        self.assertEqual(count, 2)
        self.assertEqual(workflows.decisions(destination)['globs'],
                         [str(problem), str(destination/'own.p')])
        rows = workflows.imported_rows(destination/'jobpairs.csv')
        self.assertEqual({row['problem'] for row in rows}, {str(problem), str(destination/'own.p')})

    def test_editor_rename_write_failure_restores_bytes_modes_and_name(self):
        before = self.snapshot(self.directory)
        data = workflows.decisions(self.directory)
        data.update(name='renamed', batch_size=9)
        data['configurations'][0]['solver_name'] = 'Changed'
        real_replace = Path.replace
        def fail(path, target):
            if target.name == 'Makefile': raise OSError('injected commit failure')
            return real_replace(path, target)
        with patch.object(Path, 'replace', fail), self.assertRaisesRegex(OSError, 'injected'):
            workflows.rename_and_save(data, self.directory)
        self.assertEqual(self.snapshot(self.directory), before)
        self.assertFalse((self.users/'renamed').exists())
        self.assertFalse(list(self.users.glob('.workflow-write-*')))

    def test_editor_rename_staging_failure_leaves_original_untouched(self):
        before = self.snapshot(self.directory)
        data = workflows.decisions(self.directory); data['name'] = 'renamed'
        real_write = Path.write_text
        def fail(path, *args, **kwargs):
            if path.name == 'Makefile' and path.parent.name == 'new':
                raise OSError('injected staging failure')
            return real_write(path, *args, **kwargs)
        with patch.object(Path, 'write_text', fail), self.assertRaisesRegex(OSError, 'injected'):
            workflows.rename_and_save(data, self.directory)
        self.assertEqual(self.snapshot(self.directory), before)
        self.assertFalse((self.users/'renamed').exists())

    def test_same_name_save_failure_restores_all_definitions(self):
        before = self.snapshot(self.directory)
        data = workflows.decisions(self.directory)
        data['configurations'][0]['solver_name'] = 'Changed'
        real_replace = Path.replace
        def fail(path, target):
            if target.name == 'Makefile': raise OSError('injected save failure')
            return real_replace(path, target)
        with patch.object(Path, 'replace', fail), self.assertRaisesRegex(OSError, 'injected'):
            workflows.rename_and_save(data, self.directory)
        self.assertEqual(self.snapshot(self.directory), before)
        self.assertFalse(list(self.users.glob('.workflow-write-*')))

    def test_failed_new_workflow_save_removes_partial_directory(self):
        data = workflows.decisions(self.directory); data['name'] = 'new-copy'
        real_replace = Path.replace
        def fail(path, target):
            if target.name == 'Makefile': raise OSError('injected save failure')
            return real_replace(path, target)
        with patch.object(Path, 'replace', fail), self.assertRaisesRegex(OSError, 'injected'):
            workflows.save(data)
        self.assertFalse((self.users/'new-copy').exists())
        self.assertFalse(list(self.users.glob('.workflow-write-*')))

    def test_portable_bracketed_problem_can_be_saved_and_exported_again(self):
        problem = self.root/'case[1]?.p'; problem.write_text('cnf(a,axiom,p).')
        data = workflows.decisions(self.directory); data['globs'] = [str(self.root/'case*.p')]
        workflows.save(data, self.directory)
        with patch.object(portable.subprocess, 'run', side_effect=AssertionError('No SSH allowed')):
            with portable.export_bundle(self.directory, 'datalab') as archive:
                destination, _ = portable.import_bundle(archive, 'imported')
            data = workflows.decisions(destination)
            _, count = workflows.save(data, destination)
            self.assertEqual(count, 1)
            with portable.export_bundle(destination, 'datalab') as archive:
                self.assertTrue(archive.read(1))

    def test_portable_relocates_assignment_and_option_values_end_to_end(self):
        data = workflows.decisions(self.directory)
        data['configurations'][0]['command'] = (f'TPTP={self.runtime} ./solver '
                                                f'--data={self.runtime}/data {{'+'{problem}}')
        workflows.save(data, self.directory)
        with patch.object(portable.subprocess, 'run', side_effect=AssertionError('No SSH allowed')):
            with portable.export_bundle(self.directory, 'datalab') as archive:
                destination, _ = portable.import_bundle(archive, 'imported')
        row, = workflows.imported_rows(destination/'jobpairs.csv')
        self.assertNotIn('='+str(self.runtime), row['command'])
        self.assertIn('TPTP='+row['solver_directory'], row['command'])
        self.assertIn('--data='+row['solver_directory']+'/data', row['command'])

    def test_command_relocation_quoting_boundaries_and_non_cascading(self):
        mapping = {'/old/root': '/new location/root', '/new location': '/elsewhere'}
        command = 'TPTP="/old/root" ./solver --data=/old/root/data /old/root-other /prefix/old/root'
        actual = portable.command_paths(command, mapping)
        self.assertEqual(actual, "TPTP='/new location/root' ./solver --data='/new location/root/data' /old/root-other /prefix/old/root")
