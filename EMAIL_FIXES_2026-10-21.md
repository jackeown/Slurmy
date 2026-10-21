# Gmail issues addressed on 2026-10-21

Sources: Desktop PDFs “Slurmy is less broken” and “Slurmy failure message”.

## Solver core limits and dedicated sockets/nodes

Mixed batches previously inherited their strongest socket/node exclusivity.
For example, interleaving a shared 8-core solver with a socket-isolated 12-core
solver could cause both to receive socket-sized allocations. New batches group
matching core counts, threads per core, socket requirements, and isolation modes.
Compatible interleaved requests still fill batches up to the requested batch size;
dedicated requests do not force one batch per solver-problem pair.

The solver's CPU affinity remains limited to the requested core count. Isolating
a whole 32-core socket for a 12-core solver prevents other tasks from using the
remaining cores but does not give the solver those extra cores. Whole-node
isolation works the same way. Cluster status now explicitly shows the solver's
core limit, SLURM's allocated physical cores, and the sharing/socket/node mode.
New submissions publish these requests in queue metadata so they are visible
across users; older jobs can show requested cores when their manifest is readable.
Actual allocated-core counts always come from SLURM, not the requested limit.

Existing submitted jobs keep their original batch plans. Use Prepare & dispatch
for a new job to apply the allocation grouping fix. Resume continues unfinished
pairs in the existing job and does not silently rewrite its plans.

## False resume failure

Resume prints the existing Slurmy job ID. The web operation handler incorrectly
processed it as a new workflow submission, read absent SLURMY_HOST state, and
replaced a successful operation log with a KeyError. New-job announcements are
now restricted to new submissions in both worker completion and polling. Resume
has its own operation heading, retains command output, and does not create a
second history entry. Stop/download/cancel headings are also action-specific.

## Preview, save, prepare, and dispatch

| Action | Result | Builds or submits? |
| --- | --- | --- |
| Preview files (optional) | Validate current settings and show generated files without saving a workflow | No |
| Save workflow | Independently validate and save the reusable definition; preview is unnecessary | No |
| Prepare only | Refresh submission files and batch plans for a saved workflow | No |
| Prepare & dispatch job | Prepare, build declared resources, download artifacts, and submit | Yes |
| Dispatch prepared job | Build and submit using already prepared plans | Yes |

The builder's Review step now presents this sequence and explicitly labels the
preview optional. Existing workflow submission controls retain their explanation.

## Verification

All 45 regression tests pass, including the exact resume-KeyError reproduction,
old/new queue metadata parsing, interleaved shared/dedicated batch grouping, and
an executed Linux affinity test for shared/socket/node modes.

Live job: `Email-isolation-and-resume-verification_1792579834_65621` on datalab / CPU-amd.
Three configurations ran six small TPTP problems each using real E:

| Configuration | Isolation | Requested solver cores | Measured solver affinity | Planned batches |
| --- | --- | ---: | ---: | ---: |
| Shared8 | Shared node/sockets | 8 | 8 | 2 × 3 pairs |
| Socket12 | Dedicated socket | 12 | 12 | 2 × 3 pairs |
| Node12 | Dedicated node | 12 | 12 | 2 × 3 pairs |

Every successful solver output recorded its actual CPU affinity before running E.
All 18 returned Theorem with exactly their requested core count. The job was
stopped with 12 completed pairs, then resumed through the web API. All 12 saved
result archives were preserved; the remaining six completed. Resume reported
`done` with the correct heading and no false failure. Raw verification evidence
is in the ignored local job-results directory under the job ID above.

Browser checks covered the revised node dropdown/task-row allocation descriptions
and the builder Review guidance. Only our verification job was stopped/resumed.
