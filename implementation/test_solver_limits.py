"""Run the real result publisher with solver-enforced limits and normal results."""
import csv
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

TEMPLATES=Path(__file__).parent/'templates'


class SolverLimitTests(unittest.TestCase):
    def run_pair(self, output, expected, code=1, stderr=False):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name in ['calls/0','rootfs/runtime','results/index','progress']:(root/name).mkdir(parents=True)
            shutil.copy2(TEMPLATES/'csv.sh',root/'csv.sh')
            (root/'calls/0/config.sh').write_text('TASK_ID=0\nTASK_KEY=abcdef\nCORES=1\nTHREADS_PER_CORE=1\nWC_LIMIT=3\nSOLVER_ROOT_REL=runtime\n')
            (root/'payload').write_text(output)
            (root/'timed_call.sh').write_text('''#!/bin/bash
cp "$JOB_DIR/payload" "${'''+('SOLVER_STDERR_LOG' if stderr else 'SOLVER_LOG')+'''}"
touch "$SOLVER_LOG" "$SOLVER_STDERR_LOG"
printf 'TIMEOUT=false\\nMEMOUT=false\\nWCTIME=1.01\\nCPUTIME=0.9\\n' > "$VAR_FILE"
printf 'Child status: '''+str(code)+'''\\n' > "$WATCHER_LOG"
exit '''+str(code)+'\n')
            result=subprocess.run(['bash',str(TEMPLATES/'call.sh'),'0','0'],env={**os.environ,'JOB_DIR':str(root)},capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)
            with (root/'results/batch_000000_task_000000000.csv').open() as stream:row=next(csv.DictReader(stream))
            self.assertEqual(row['status'],expected)
            self.assertEqual(row['complete'],'true')
            return row

    def test_vampire_internal_timeout_on_stdout_or_stderr(self):
        for stderr in (False,True):
            with self.subTest(stderr=stderr):
                row=self.run_pair('0.80/1.00\t% Termination reason: Time limit\n','time-limit',stderr=stderr)
                self.assertEqual(row['timed_out'],'true')

    def test_szs_internal_limits_set_resource_flags(self):
        row=self.run_pair('% SZS status Timeout\n','time-limit');self.assertEqual(row['timed_out'],'true')
        row=self.run_pair('% SZS status MemoryOut\n','memory-limit');self.assertEqual(row['memory_out'],'true')
        row=self.run_pair('% Termination reason: Memory limit\n','memory-limit');self.assertEqual(row['memory_out'],'true')

    def test_unrelated_text_and_success_are_not_overridden(self):
        self.run_pair('syntax error around Time limit\n','solver-error')
        row=self.run_pair('% Termination reason: Time limit\n% SZS status Theorem\n','ok',code=0)
        self.assertEqual(row['timed_out'],'false');self.assertEqual(row['szs_status'],'Theorem')
