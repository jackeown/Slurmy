"""Build reuse invalidation and in-place downloads."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('build_driver', Path(__file__).parent/'build/slurmy-build.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class BuildCacheTests(unittest.TestCase):
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
