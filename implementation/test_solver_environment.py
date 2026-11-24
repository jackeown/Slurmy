"""Literal environment syntax and safe shell emission."""
import subprocess
import shlex
import unittest
from solver_environment import parse, exports


class SolverEnvironmentTests(unittest.TestCase):
    def test_assignments_quotes_whitespace_and_empty_values(self):
        self.assertEqual(parse('X=Y Z=3\nHello = World\nEMPTY= NEXT = "hello world"\nEQ="a=b"'),
                         dict(X='Y',Z='3',Hello='World',EMPTY='',NEXT='hello world',EQ='a=b'))
        self.assertEqual(parse("A='one two' B=three\\ four C= D=''"),dict(A='one two',B='three four',C='',D=''))
        self.assertEqual(parse(' \n\t'),{})

    def test_invalid_names_duplicates_quotes_and_control_characters(self):
        for text in ['1A=x','A-B=x','A x','A=x A=y','A="x',"A='x",'A=x\\','A=\x00','A=x\nmissing']:
            with self.subTest(text=text),self.assertRaises(ValueError):parse(text)

    def test_values_remain_literal_when_exported(self):
        values=parse('A="$(echo unsafe); `echo unsafe` $HOME" B="a b"')
        result=subprocess.run(['bash','-c',exports(values)+'printf "%s\\n" "$A" "$B"'],capture_output=True,text=True,check=True)
        self.assertEqual(result.stdout.splitlines(),[values['A'],values['B']])

    def test_placeholder_fragments_are_safely_joined_with_literal_text(self):
        values=parse('THREADS={{cores}} INPUT={{problem}} MESSAGE="before {{problem}} after; $(echo BAD)"')
        path="/cluster/a b'$(echo BAD).p"
        script=exports(values,dict(cores='12',problem=shlex.quote(path)))+'printf "%s\\n" "$THREADS" "$INPUT" "$MESSAGE"'
        result=subprocess.run(['bash','-c',script],capture_output=True,text=True,check=True)
        self.assertEqual(result.stdout.splitlines(),['12',path,'before '+path+' after; $(echo BAD)'])

    def test_unknown_malformed_or_unavailable_placeholders_fail(self):
        for text in ['X={{unknown}}','X={{solver_command}}','X={{cores','X={{ cores }}','X=cores}}']:
            with self.subTest(text=text),self.assertRaises(ValueError):parse(text)
        with self.assertRaisesRegex(ValueError,'not available'):exports(parse('X={{cpus}}'),{})
