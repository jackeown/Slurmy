"""Build reuse invalidation and in-place downloads."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import importlib.util
from pathlib import Path
import tempfile
import json
import os
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('build_driver', Path(__file__).parent.parent/'build/slurmy-build.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class BuildCacheTests(unittest.TestCase):
    def test_python_build_script_runs_with_literal_arguments_and_affects_cache(self):
        self.recipe.write_text('#!/usr/bin/env python3\nimport os,json,sys\nfrom pathlib import Path\nPath(os.environ["SLURMY_BUILD_OUTPUT"],"solver").write_text(json.dumps(sys.argv[1:]))\n')
        self.args.recipe_arg=['--configuration=fast','a value with spaces']
        before=driver.input_fingerprint(self.args)
        self.args.recipe_arg.append('--extra')
        self.assertNotEqual(before,driver.input_fingerprint(self.args))
        stage=self.root/'cluster-build';stage.mkdir()
        (stage/'recipe.sh').write_text(self.recipe.read_text())
        script=stage/'job.sh';script.write_text(driver.build_job_script(self.args,has_context=False))
        subprocess.run(['bash',str(script)],env={**os.environ,'SLURM_SUBMIT_DIR':str(stage)},capture_output=True,text=True,check=True)
        self.assertEqual(json.loads((stage/'output/solver').read_text()),self.args.recipe_arg)

    def test_socket_build_requests_physical_cores_and_sets_parallelism(self):
        self.args.cpus_per_task='socket'
        self.args.sbatch_option=['--partition=CPU-amd']
        with patch.object(driver,'dedicated_core_topology',return_value=(2,32)) as topology:
            script=driver.build_job_script(self.args,has_context=False)
        topology.assert_called_once_with(self.args.host,'CPU-amd')
        self.assertIn('#SBATCH --cpus-per-task=32',script)
        self.assertIn('#SBATCH --threads-per-core=1',script)
        stage=self.root/'parallel-build';stage.mkdir()
        (stage/'recipe.sh').write_text('printf "%s\\n" "$SLURMY_BUILD_CORES" "$CMAKE_BUILD_PARALLEL_LEVEL" "$MAKEFLAGS" > "$SLURMY_BUILD_OUTPUT/solver"')
        job=stage/'job.sh';job.write_text(script)
        subprocess.run(['bash',str(job)],env={**os.environ,'SLURM_SUBMIT_DIR':str(stage),'SLURM_CPUS_PER_TASK':'32','MAKEFLAGS':''},capture_output=True,text=True,check=True)
        self.assertEqual((stage/'output/solver').read_text().splitlines(),['32','32',' -j32'])

    def test_socket_build_requires_partition_and_reports_lookup_failure(self):
        self.args.cpus_per_task='socket'
        with self.assertRaisesRegex(driver.BuildError,'require'):driver.build_job_script(self.args,has_context=False)
        self.args.sbatch_option=['--partition=CPU-amd']
        with patch.object(driver,'dedicated_core_topology',side_effect=ValueError('offline')):
            with self.assertRaisesRegex(driver.BuildError,'offline'):driver.build_job_script(self.args,has_context=False)

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.source = self.root/'source'
        self.source.mkdir()
        (self.source/'code.c').write_text('original')
        self.recipe = self.root/'recipe.sh'
        self.recipe.write_text('make')
        self.output = self.root/'output'
        self.args = driver.build_parser().parse_args(['--name', 'test', '--recipe', str(self.recipe), '--context', str(self.source), '--output', str(self.output), '--artifact', 'solver', '--build-policy', 'changed'])

    def build(self, args):
        args._inputs_unchanged = driver.input_fingerprint(args) == args._initial_fingerprint
        (Path(args.output)/'solver').write_text('built')

    def test_reuse_and_invalidation(self):
        with patch.object(driver, 'execute_build', side_effect=self.build) as build:
            driver.cached_build(self.args)
            driver.cached_build(self.args)
            self.assertEqual(build.call_count, 1)
            for change in (lambda: (self.source/'code.c').write_text('edited'),
                           lambda: (self.source/'extra.c').write_text('new'),
                           lambda: (self.source/'extra.c').unlink(),
                           lambda: self.recipe.write_text('make different'),
                           lambda: (self.output/'solver').unlink(),
                           lambda: (self.output/'solver').write_text('corrupt')):
                count = build.call_count
                change()
                driver.cached_build(self.args)
                self.assertEqual(build.call_count, count+1)
                driver.cached_build(self.args)
                self.assertEqual(build.call_count, count+1)
            self.args.build_policy = 'always'
            count = build.call_count
            driver.cached_build(self.args)
            driver.cached_build(self.args)
            self.assertEqual(build.call_count, count+2)

    def test_in_place_build_reuses_post_download_tree(self):
        self.args.output = str(self.source)
        self.args.in_place = True
        with patch.object(driver, 'execute_build', side_effect=self.build) as build:
            driver.cached_build(self.args)
            driver.cached_build(self.args)
            self.assertEqual(build.call_count, 1)

    def test_failed_build_invalidates_receipt(self):
        with patch.object(driver, 'execute_build', side_effect=self.build):
            driver.cached_build(self.args)
        self.args.build_policy = 'always'
        with patch.object(driver, 'execute_build', side_effect=driver.BuildError('failed')):
            with self.assertRaises(driver.BuildError):
                driver.cached_build(self.args)
        self.assertFalse((self.output/'.slurmy-build-cache.json').exists())

    def test_missing_build_download_tool_is_reported_before_remote_work(self):
        with patch.object(driver.shutil, 'which', side_effect=lambda tool: None if tool == 'rsync' else '/tool/' + tool), \
             patch.object(driver, 'run') as remote:
            with self.assertRaisesRegex(driver.BuildError, 'local tools: rsync'):
                driver.execute_build(None)
            remote.assert_not_called()
