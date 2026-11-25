"""Official SZS vocabularies, language parity, exports, and unknown values."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import json
from pathlib import Path
import subprocess
import unittest
import szs
from starexec_export import summary_row


class SZSTests(unittest.TestCase):
    def test_complete_official_catalog_and_bidirectional_case_insensitive_identifiers(self):
        self.assertEqual((len(szs.SUCCESS),len(szs.NO_SUCCESS),len(szs.DATA_FORMS)),(53,29,31))
        self.assertEqual(len(set(szs.STATUSES.values())),82)
        for name,code in szs.STATUSES.items():
            for value in (name,code,name.lower(),code.lower()):
                with self.subTest(value=value):
                    self.assertEqual(szs.canonical(value),name)
                    self.assertEqual(szs.mnemonic(value),code)
                    self.assertEqual(szs.successful(value),name in szs.SUCCESS)

    def test_resource_failures_and_unsuccessful_processing_are_not_logical_success(self):
        for value in ['TMO','CTO','WTO','Timeout','CPUTimeout','WCTimeout']:
            self.assertEqual(szs.execution_status(value),'time-limit')
        self.assertEqual(szs.execution_status('MMO'),'memory-limit')
        self.assertEqual(szs.execution_status('RSO'),'resource-limit')
        for value in ['GUP','UNK','INC','IAP','INP','NTT','NTY','NOS','FOR','USR','STP','OPN','NVE']:
            self.assertEqual(szs.normalize_execution('ok',value),'no-success')
        for value in ['ERR','OSE','INE','SYE','SEE','TYE','USM','USE','ICT','FVE']:
            self.assertEqual(szs.normalize_execution('ok',value),'solver-error')
        self.assertEqual(szs.normalize_execution('interrupted','THM','143'),'interrupted')
        self.assertEqual(szs.normalize_execution('solver-error','THM','143'),'solver-error')

    def test_assumptions_do_not_turn_an_unknown_answer_into_a_proof(self):
        value='ASS( TMO , THM )'
        self.assertEqual(szs.canonical(value),'Assumed(Timeout,Theorem)')
        self.assertEqual(szs.mnemonic(value),'ASS(TMO,THM)')
        self.assertFalse(szs.successful(value))
        self.assertEqual(szs.execution_status(value),'no-success')
        self.assertEqual(szs.mnemonic('ASS(THM,UNK)'),'')
        self.assertEqual(szs.STATUS_RE.findall('% szs STATUS '+value+' for x'),[value])

    def test_data_forms_do_not_confuse_status_codes_with_output_codes(self):
        self.assertEqual(szs.canonical('SAT'),'Satisfiable')
        self.assertEqual(szs.data_form('Sat'),'SaturationHerbrandModel')
        self.assertEqual(szs.canonical('ASS'),'Assumed')
        self.assertEqual(szs.data_form('Ass'),'Assurance')
        for name,code in szs.DATA_FORMS.items():
            self.assertEqual(szs.data_form(code),name)
            self.assertEqual(szs.data_mnemonic(name),code)

    def test_bash_and_python_agree_for_every_name_mnemonic_and_assumed_status(self):
        values=[alias for name,code in szs.STATUSES.items() for alias in (name,code)] + ['ASS(UNK,THM)']
        script=szs.shell_library()+'\nfor value in "$@"; do printf "%s\\t%s\\n" "$(szs_status_name "$value")" "$(szs_execution "$value")"; done\n'
        result=subprocess.run(['bash','-c',script,'test',*values],capture_output=True,text=True,check=True)
        self.assertEqual(result.stdout.splitlines(),[szs.canonical(value)+'\t'+szs.execution_status(value) for value in values])

    def test_starexec_exports_all_codes_and_preserves_cpu_vs_wall_timeout(self):
        for name,code in szs.STATUSES.items():
            row=summary_row(dict(complete='true',status='ok',szs_status=code,szs_output='Prf'),'problem')
            self.assertEqual(row['SZSStatus'],code)
            self.assertEqual(row['result'],name)
            self.assertEqual(row['SZSOutput'],'Prf')
        for value,expected in [('CTO','timeout (cpu)'),('WTO','timeout (wallclock)')]:
            self.assertEqual(summary_row(dict(complete='true',status='ok',szs_status=value),'p')['status'],expected)
        assumed=summary_row(dict(complete='true',status='ok',szs_status='ASS(UNK,THM)'),'p')
        self.assertEqual(assumed['SZSStatus'],'ASS')
        self.assertEqual(assumed['result'],'Assumed(Unknown,Theorem)')
        self.assertEqual(szs.canonical('FutureStatus'),'FutureStatus')
        self.assertFalse(szs.successful('FutureStatus'))

    def test_verbose_summary_counts_success_without_counting_assumptions_or_unfinished_proofs(self):
        import csv,importlib.util,tempfile
        from remote_export import RESULT_FIELDS
        spec=importlib.util.spec_from_file_location('szs_sync_test',Path(__file__).parent.parent/'slurmy-sync.py')
        sync=importlib.util.module_from_spec(spec);spec.loader.exec_module(sync)
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'results').mkdir()
            values=[*szs.STATUSES.values(),'ASS(UNK,THM)','THM']
            (root/'metadata.json').write_text(json.dumps({'task_count':len(values)}))
            with (root/'results/batch_test.csv').open('w') as stream:
                writer=csv.DictWriter(stream,fieldnames=RESULT_FIELDS);writer.writeheader()
                for task,value in enumerate(values):
                    writer.writerow(dict(task_id=task,complete='true' if task<83 else 'false',
                                         status='ok' if task<83 else 'interrupted',szs_status=value))
            summary=sync.build_summary(host='test',job_id='job',destination=root,changed_paths=0,iteration=1,cache=sync.ArchiveStatusCache())
        self.assertEqual(summary['completed_tasks'],83)
        self.assertEqual(summary['solved_tasks'],53)
        self.assertEqual(summary['szs_status_counts']['Assumed(Unknown,Theorem)'],1)
