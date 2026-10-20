# Verification of the email's batching example

Verified on datalab / CPU-amd on 2026-10-20 with a real SLURM run:
`Email-batching-verification_1792511421_490491`.

The test workflow is `workflows/my-workflows/email-batch-verification-20261020`.
It uses two real E configurations (`--auto` and default), 1,429 distinct small
TPTP problems from one `.tgz`, solver-major order, batch size 500, eight physical
cores per configuration, and shared nodes/sockets. These are synthetic, easy
proof problems for checking scheduling and accounting, not a solver benchmark.

## Discrepancy found and fixed

The previous planner filled batches across configuration boundaries. For this
example it produced six batches of 500, 500, 500, 500, 500, and 358 pairs;
batch 2 mixed 429 pairs from the first configuration with 71 from the second.
That met the overall count but did not match the email's three-batches-per-solver
expectation.

Solver-major planning now starts a new batch at a configuration boundary.
Workflow Makefiles pass the selected pair order to the planner, previews use the
same rule, and portable imports retain the order. Problem-major and random order
can still mix configurations. Pairs remain sequential within every batch.

## Observed results

| Batch | Configuration | Planned pairs | Successful pairs | SLURM task | Node | Final state |
| --- | --- | ---: | ---: | --- | --- | --- |
| 0 | E-auto | 500 | 500 | 894843_0 | os-cpu-slurm-18 | COMPLETED |
| 1 | E-auto | 500 | 500 | 894844_0 | os-cpu-slurm-18 | COMPLETED |
| 2 | E-auto | 429 | 429 | 894845_0 | os-cpu-slurm-18 | COMPLETED |
| 3 | E-default | 500 | 500 | 894846_0 | os-cpu-slurm-18 | COMPLETED |
| 4 | E-default | 500 | 500 | 894847_0 | os-cpu-slurm-17 | COMPLETED |
| 5 | E-default | 429 | 429 | 894848_0 | os-cpu-slurm-17 | COMPLETED |

All six tasks exited with `0:0`; elapsed times ranged from 1:30 to 2:01.
Remote plans, batch scripts, metadata, and submission records each contained
exactly six batches. All 2,858 pair IDs appeared exactly once, with 1,429 distinct
problems per configuration, all returning `Theorem`. Every batch allocated eight
physical cores and ran one pair at a time without node or socket exclusivity.

The workflow API and browser showed the correct total of 2,858 during execution.
The final API reported `DONE`, `2858 / 2858`, 100%, six submitted batches, zero
issues, and no provisional dispatching row. Preparation was deliberately seeded
with obsolete metadata containing 26,990 pairs to exercise the display fix.

The downloaded summary has exactly 2,858 rows. Both stdout and stderr archives
contain 2,858 log files, retaining the original `SET0000+test_case.p` style names
and the triple-underscore field separators.

Downloaded evidence is under
`job-results/Email-batching-verification_1792511421_490491/`, including
`verification.json` (remote assertions and SLURM accounting), `workflow-api.json`,
the results summary CSV, and both log archives. Test inputs and downloaded
results remain local and are ignored by Git.

## Automated regression checks

All 36 tests passed (27 web tests and 9 implementation tests), including the
1,429-problem archive reproduction, configuration boundaries, mixed batching
with socket/node exclusivity, portable order preservation, and dispatch progress.

```sh
python -m unittest discover -s web-app -p 'test_*.py'
python -m unittest discover -s implementation -p 'test_*.py'
```
