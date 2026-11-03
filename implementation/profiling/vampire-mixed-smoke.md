# Mixed Vampire Smoke Test — 3 November 2026

Live execution on datalab, partition CPU-amd, using the local prebuilt Vampire
5.1.0 executable and local prebuilt runsolver 3.4.1. No remote solver compilation.
One physical core, one thread per core, shared placement, 1 GiB per pair,
one-second CPU and wall limits, two pairs per batch, three outstanding tasks.

The local workflow `workflows/my-workflows/vampire-mixed-smoke-20261103` contains
three cluster-local inputs (TPTP-v9.2.1/Problems/PUZ/PUZ001+1.p, PUZ002-1.p,
PUZ003-1.p) and three local examples (satisfiable CNF, a simple theorem, and
contradictory CNF). A fourth local example exercises the timeout: a propositional
11-pigeon/10-hole encoding with 561 clauses. The final command logs the problem's
SHA-256 and line count before executing Vampire. The workflow and downloaded
results are local ignored data, not committed binaries or generated output.

## Observed Results

Initial six-pair job `vampire-mixed-smoke-20261103_1793705160_956682` completed in
three batches. All six expected results were obtained: one Satisfiable, two
Theorem, and three Unsatisfiable. Standard export produced the summary and two
log archives. Filenames preserved underscores, plus signs, and carets with triple
underscores between fields. Archives contained their own named parent directory.

Final seven-pair job `vampire-mixed-smoke-20261103_1793705447_957159` completed in
four batches, all four accepted. The same six inputs returned the expected
statuses. The pigeonhole problem was recorded as `time-limit` at 1.0519 seconds.
The CSV records both configured limits as 1. All seven results were downloaded.
Its stdout checksum matched the local input:
`205bcd00eec6c65afd229b1fc00c7d1cec73b6b0febebf4c1f7293cfff9dba37`.
The preceding uninstrumented repeat (`...1793705389_957069`) also produced the
expected timeout, at 1.0307 seconds.

Standard results are under `job-results/JOB_ID`. Verbose diagnostics for the
anomalous run below are under `job-results/verbose/JOB_ID`. The final workflow
can be prepared and submitted again using `make prepare-submit` in its directory.

## Unresolved Solver Outcome Anomaly

The first seven-pair run (`vampire-mixed-smoke-20261103_1793705189_956748`)
unexpectedly reported Satisfiable for the pigeonhole input in 0.347696 seconds,
with only 0.012647 CPU seconds. This was present in Vampire's raw stdout; it was
not introduced by Slurmy's status parser. No definitive cause was established.
After execution, the uploaded binary and problem matched the local SHA-256 hashes.
This does not prove the bytes seen by the original solver process at runtime.

Separate compute-node diagnostic SLURM jobs 953171 and 953176 ran the same
binary/input directly, from a scratch copy, and through the generated limiter
wrapper. All three paths reached the expected one-second timeout. No solver work
was run on the login node. Both subsequent full Slurmy submissions also timed out
correctly, including the final run with runtime input checksums.

Do not interpret the successful repeats as a fix for the earlier result. Preserve
its raw logs for further investigation if the discrepancy recurs. No speculative
change to result classification, input handling, or the solver was made.
