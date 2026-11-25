"""Expose a real scheduler rejection without retrying an ambiguous submission."""
import _bootstrap  # noqa: F401
import io
import json
import unittest
from unittest.mock import patch
import urllib.error
import slurm_api


class ApiDiagnosticsTests(unittest.TestCase):
    def test_http_error_preserves_real_reservation_cause_and_code(self):
        body={'errors':[{'description':'Batch job submission failed','error_number':2053,
                         'error':'Requested reservation is invalid','source':'slurm_submit_batch_job()'}]}
        failure=urllib.error.HTTPError(slurm_api.DEFAULT_URL,500,'failed',{},io.BytesIO(json.dumps(body).encode()))
        with patch.object(slurm_api,'credentials',return_value={'token':'secret','user':'user'}),patch.object(slurm_api.urllib.request,'build_opener') as opener:
            opener.return_value.open.side_effect=failure
            with self.assertRaises(RuntimeError) as caught:slurm_api.request('host','job/submit','POST',{})
        message=str(caught.exception)
        self.assertIn('Requested reservation is invalid',message)
        self.assertIn('Batch job submission failed',message)
        self.assertIn('2053',message);self.assertIn('Resume retains',message)
        opener.return_value.open.assert_called_once()
        self.assertIn('No SSH fallback',message)

    def test_success_status_error_response_uses_same_diagnostics(self):
        body={'errors':[{'error':'Invalid account','description':'Batch job submission failed','error_number':123}]}
        with patch.object(slurm_api,'credentials',return_value={'token':'secret','user':'user'}),patch.object(slurm_api.urllib.request,'build_opener') as opener:
            opener.return_value.open.return_value=io.BytesIO(json.dumps(body).encode())
            with self.assertRaisesRegex(RuntimeError,'Invalid account.*Batch job submission failed.*123'):
                slurm_api.request('host','job/submit','POST',{})

    def test_tokens_are_redacted_before_truncation_and_duplicates_are_removed(self):
        text=slurm_api.error_details([{'error':'secret failure','description':'secret failure','error_number':2}],'secret')
        self.assertNotIn('secret',text);self.assertEqual(text.count('[redacted] failure'),1)
        self.assertEqual(slurm_api.error_details(None),'')
        self.assertEqual(slurm_api.error_details(['bad shape']),'')

    def test_ambiguous_network_failure_is_not_retried_or_sent_via_ssh(self):
        with patch.object(slurm_api,'credentials',return_value={'token':'secret','user':'user'}),patch.object(slurm_api.urllib.request,'build_opener') as opener:
            opener.return_value.open.side_effect=urllib.error.URLError('connection reset')
            with self.assertRaisesRegex(RuntimeError,'No SSH fallback'):
                slurm_api.request('host','job/submit','POST',{})
            opener.return_value.open.assert_called_once()
