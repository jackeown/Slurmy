"""StarExec exports mirror the supplied archives without changing other downloads."""
import argparse
import csv
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from starexec_export import FIELDS, export, job_number, summary_row
from remote_export import EXPORT_FIELDS, stream_name


class StarExecExportTests(unittest.TestCase):
    def fixture(self, root):
        source=root/'compact';source.mkdir()
        rows=[]
        for task,problem,status,szs in [(0,'/share/TPTP/TPTP-v9.2.1/Problems/SET/SET002+3.p','ok','Theorem'),
                                       (1,'/share/TPTP/TPTP-v9.2.1/Problems/SET/SET002^3.p','time-limit',''),
                                       (2,'/local/a/local_name.p','',''),(3,'/local/b/local_name.p','solver-error','Error')]:
            row={key:'' for key in EXPORT_FIELDS}
            row.update(task_id=str(task),problem=problem,solver='Vampire_config',complete='false' if task==2 else 'true',status=status,
                       szs_status=szs,szs_output='Proof' if task==0 else '',cpu_seconds='1.25',wall_seconds='2.5',cpu_limit='1',max_memory_kib='2048')
            rows.append(row)
        with (source/'test_results-summary.csv').open('w',newline='') as output:
            writer=csv.DictWriter(output,fieldnames=EXPORT_FIELDS);writer.writeheader();writer.writerows(rows)
        for error in [False,True]:
            suffix='stderr' if error else 'stdout'
            with tarfile.open(source/f'test_solver-{suffix}.tgz','w:gz') as archive:
                for row in rows:
                    task=int(row['task_id'])
                    if task==2:continue
                    content=(b'error diagnostic\n' if error else b'% SZS status Theorem\n0.10/0.20\tpre-timestamped\nproof\n')
                    member=tarfile.TarInfo('test/'+stream_name(row,task,error));member.size=len(content)
                    archive.addfile(member,io.BytesIO(content))
        return source

    def test_schema_units_filenames_and_outputs_match_reference_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=self.fixture(root);target=root/'starexec'
            names,done,total=export(source,target,'test-job')
            self.assertEqual((done,total),(3,4))
            prefix='Job'+str(job_number('test-job'))
            self.assertEqual(names,(prefix+'_info.zip',prefix+'_output.zip'))
            with zipfile.ZipFile(target/names[0]) as z:
                self.assertEqual(z.namelist(),[prefix+'/'+prefix+'_info.csv'])
                reader=csv.DictReader(io.StringIO(z.read(z.namelist()[0]).decode()))
                self.assertEqual(tuple(reader.fieldnames),FIELDS);rows=list(reader)
            self.assertEqual(rows[0]['benchmark'],'Problems/SET/SET002+3.p')
            self.assertEqual(rows[1]['benchmark'],'Problems/SET/SET002^3.p')
            self.assertEqual(rows[0]['memory usage'],'2097152')
            self.assertEqual(rows[0]['SZSResult'],'THM-Prf')
            self.assertEqual(rows[1]['status'],'timeout (cpu)')
            self.assertEqual(rows[2]['status'],'incomplete');self.assertEqual(rows[2]['SZSStatus'],'-')
            self.assertEqual(rows[3]['status'],'error');self.assertNotEqual(rows[2]['benchmark'],rows[3]['benchmark'])
            self.assertEqual(rows[0]['expected'],'--')
            with zipfile.ZipFile(target/names[1]) as z:
                self.assertEqual(len(z.namelist()),3)
                name=prefix+'_output/Problems/SET/Vampire_config___Vampire_config/SET002+3.p/0.txt'
                text=z.read(name).decode()
                self.assertIn('1.25/2.5\t% SZS status Theorem',text)
                self.assertIn('0.10/0.20\tpre-timestamped',text)
                self.assertIn('stderr follows stdout',text);self.assertIn('error diagnostic',text)
                self.assertTrue(text.endswith('EOF\n'))
            # Repeat uses stable job/pair IDs and leaves standard downloads intact.
            self.assertEqual(export(source,target,'test-job')[0],names)
            self.assertEqual(len(list(source.iterdir())),3)

    def test_failed_conversion_keeps_previous_zips(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);source=self.fixture(root);target=root/'starexec'
            names,*_=export(source,target,'test-job');before={n:(target/n).read_bytes() for n in names}
            (source/'test_solver-stderr.tgz').write_bytes(b'broken archive')
            with self.assertRaises(tarfile.TarError):export(source,target,'test-job')
            self.assertEqual({n:(target/n).read_bytes() for n in names},before)

    def test_missing_measurements_and_memory_limit_are_not_marked_solved(self):
        row=summary_row(dict(complete='true',status='memory-limit'), 'Problems/local/test.p')
        self.assertEqual(row['status'],'memout');self.assertEqual(row['result'],'--');self.assertEqual(row['memory usage'],'0.0')

    def test_cli_uses_existing_downloader_once_and_keeps_formats_separate(self):
        spec=importlib.util.spec_from_file_location('starexec_sync_test',Path(__file__).with_name('slurmy-sync.py'))
        sync=importlib.util.module_from_spec(spec);spec.loader.exec_module(sync)
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def download(args,resolved_job_id=None):
                self.assertEqual(resolved_job_id,'test-job');self.assertFalse(args.follow)
                staged=self.fixture(Path(args.output).parent)
                self.assertEqual(staged,Path(args.output));return 0
            args=sync.build_parser().parse_args(['test-job','--starexec','--output',str(root/'out')])
            with patch.object(sync,'resolve_job_id',return_value='test-job') as resolve,patch.object(sync,'compact_sync_loop',side_effect=download) as transfer:
                self.assertEqual(sync.starexec_sync_loop(args),0)
            resolve.assert_called_once();transfer.assert_called_once()
            self.assertEqual(len(list((root/'out').glob('*.zip'))),2)
            with self.assertRaises(SystemExit):sync.build_parser().parse_args(['--verbose','--starexec'])
