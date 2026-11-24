"""Protocol parsing and the bundled solver adapter's command line contract."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import szs
from postprocessing import parse_data

ROOT = Path(__file__).resolve().parent


class PostprocessingTests(unittest.TestCase):
    def test_literal_assignments_and_last_assignment(self):
        text = '''arbitrary solver output
% SZS data some_name = first
 SZS data some_name= second = "quoted", value
SZS data Status=UPPER
SZS data status=sat
SZS data empty =
SZS data _key.1-x = café
szs data ignored=wrong case
prefix SZS data ignored=prefix
SZS data 1bad=no
'''
        expected = {'some_name': 'second = "quoted", value', 'Status': 'UPPER',
                    'status': 'sat', 'empty': '', '_key.1-x': 'café'}
        self.assertEqual(parse_data(text), expected)
        self.assertEqual(parse_data(iter(text.splitlines(True))), expected)

    def test_key_length(self):
        self.assertEqual(parse_data('SZS data ' + 'a'*128 + '=yes'), {'a'*128: 'yes'})
        self.assertEqual(parse_data('SZS data ' + 'a'*129 + '=no'), {})

    def helper(self, text):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder)/'post.log'
            log.write_text(text)
            result = subprocess.run([sys.executable, str(ROOT/'postprocessing.py'), str(log)],
                                    capture_output=True, text=True, check=True)
        return result.stdout.splitlines()

    def test_helper_five_lines_and_unknown_raw_status(self):
        raw = 'custom = "answer", café'
        lines = self.helper(f'SZS data status={raw}\nSZS data output=alien\n')
        self.assertEqual(len(lines), 5)
        self.assertEqual(json.loads(lines[0]), {'status': raw, 'output': 'alien'})
        self.assertIn('\\u00e9', lines[0])
        self.assertEqual(lines[1:], ['', '', '', raw])

    def test_helper_canonical_and_raw(self):
        self.assertEqual(self.helper('SZS data status=sat\nSZS data output=CRf')[1:],
                         ['Satisfiable', 'CNFRefutation', 'ok', 'sat'])
        self.assertEqual(self.helper('SZS data status=ASS( TMO , THM )')[1:],
                         ['Assumed(Timeout,Theorem)', '', 'no-success', 'ASS( TMO , THM )'])
        self.assertEqual(self.helper('% SZS status Theorem\n')[1:], ['', '', '', ''])

    def adapter(self, stdout, stderr='', copied=False):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            out, err = folder/'stdout', folder/'stderr'
            out.write_text(stdout)
            err.write_text(stderr)
            script = ROOT/'postprocessors/default_szs.py'
            if copied:
                shutil.copy(script, folder/'default_szs.py')
                shutil.copy(ROOT/'szs.py', folder/'szs.py')
                script = folder/'default_szs.py'
            result = subprocess.run([sys.executable, str(script), str(out), str(err)],
                                    capture_output=True, text=True, check=True)
            return parse_data(result.stdout)

    def test_all_names_and_tlas(self):
        for name, tla in szs.STATUSES.items():
            for token in (name, tla.lower()):
                with self.subTest(token=token):
                    self.assertEqual(self.adapter(f'0.12 % SZS status {token} for problem'),
                                     {'status': name})
        for name, tla in szs.DATA_FORMS.items():
            with self.subTest(form=name):
                self.assertEqual(self.adapter(f'% SZS output start {tla} for p'), {'output': name})

    def test_mixed_logs_and_qualified_status(self):
        self.assertEqual(self.adapter('% SZS status Theorem\n% SZS output start Proof\n',
                                      '0.1 % SZS status ASS( TMO , THM ) for p\n% SZS output end CRf\n'),
                         {'status': 'Assumed(Timeout,Theorem)', 'output': 'CNFRefutation'})

    def test_solver_limits_and_status_precedence(self):
        for reason, status in [('Time', 'Timeout'), ('Memory', 'MemoryOut')]:
            self.assertEqual(self.adapter('', f'0.25 % Termination reason: {reason} limit\n'),
                             {'status': status})
        self.assertEqual(self.adapter('% SZS status THM\n', '% Termination reason: Time limit\n'),
                         {'status': 'Theorem'})
        self.assertEqual(self.adapter('', '% Termination reason: Memory limit\n% Termination reason: Time limit\n'),
                         {'status': 'Timeout'})

    def test_copied_runtime_import(self):
        self.assertEqual(self.adapter('% SZS status THM', copied=True), {'status': 'Theorem'})

    def test_cli_errors_are_nonzero_and_clear(self):
        for script, args in [('postprocessing.py', ['missing-log']),
                             ('postprocessors/default_szs.py', ['missing-out', 'missing-err']),
                             ('postprocessors/default_szs.py', []),
                             ('postprocessors/default_szs.py', ['one', 'two', 'three'])]:
            result = subprocess.run([sys.executable, str(ROOT/script), *args], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(result.stderr)
            self.assertEqual(result.stdout, '')


if __name__ == '__main__':
    unittest.main()
