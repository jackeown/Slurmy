"""About credits and version information need no cluster connection or git install."""
import _bootstrap  # noqa: F401
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from slurmy import VERSION
from slurmy_web.app import create_app


class AboutTests(unittest.TestCase):
    def setUp(self):
        with patch('slurmy_web.workflows.migrate_user_runs'):
            self.client=create_app().test_client()

    def test_about_uses_logo_and_declared_version_and_neutral_credits(self):
        with patch('slurmy_web.app.subprocess.run',return_value=SimpleNamespace(returncode=0,stdout='abc1234\n')):
            response=self.client.get('/about?host=datalab')
        self.assertEqual(response.status_code,200)
        html=response.text
        for value in (VERSION,'abc1234','Jack McKeown','TU Wien dataLAB','FORSYTE','APRe','<span>About</span>','/logo.svg'):
            self.assertIn(value,html)
        self.assertNotIn('paid for',html)
        self.assertIn('aria-current="page"',html)

    def test_about_is_available_without_git(self):
        with patch('slurmy_web.app.subprocess.run',side_effect=FileNotFoundError('git')):
            response=self.client.get('/about')
        self.assertEqual(response.status_code,200)
        self.assertIn(VERSION,response.text)
        self.assertNotIn('revision <code>',response.text)
