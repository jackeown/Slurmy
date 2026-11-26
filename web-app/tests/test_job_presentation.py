"""Job sections and status names preserve the intended display and interactions."""
import _bootstrap  # noqa: F401
from html.parser import HTMLParser
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch
from slurmy_web import app as web

STATIC=Path(__file__).parent.parent/'slurmy_web/static'


class JobPresentationTests(unittest.TestCase):
    def test_pairs_start_closed_and_batching_is_last(self):
        class Sections(HTMLParser):
            def __init__(self):
                super().__init__();self.panels=[]
            def handle_starttag(self,tag,attrs):
                attrs=dict(attrs)
                if 'panel' in attrs.get('class','').split():self.panels.append((tag,attrs))
        with patch.object(web.workflows,'migrate_user_runs'):
            app=web.create_app()
        response=app.test_client().get('/jobs/example?host=local')
        self.assertEqual(response.status_code,200)
        parser=Sections();parser.feed(response.text)
        pairs=next((tag,attrs) for tag,attrs in parser.panels if attrs.get('id')=='remote-data')
        self.assertEqual(pairs[0],'details');self.assertNotIn('open',pairs[1])
        self.assertEqual(parser.panels[-1][1]['id'],'job-batches')

    def test_status_codes_keep_full_names_and_custom_status_spelling(self):
        source=(STATIC/'app.js').read_text()
        functions=source[source.index('function szsLabel('):source.index('const otherOutcomeColors=')]
        script=r'''
const assert=require('node:assert/strict');
const szsCatalog={Theorem:{name:'Theorem',code:'THM'},ContradictoryAxioms:{name:'ContradictoryAxioms',code:'CAX'},Assumed:{name:'Assumed',code:'ASS'},Timeout:{name:'Timeout',code:'TMO'}};
'''+functions+r'''
assert.equal(szsCode('ContradictoryAxioms'),'CAX');
assert.equal(szsLabel('ContradictoryAxioms'),'ContradictoryAxioms (CAX)');
assert.equal(szsCode('Assumed(Timeout,Theorem)'),'ASS(TMO,THM)');
assert.equal(szsCode('myCustomStatus'),'myCustomStatus');
assert.equal(szsLabel('myCustomStatus'),'myCustomStatus');
'''
        subprocess.run(['node','-e',script],check=True,capture_output=True,text=True)
