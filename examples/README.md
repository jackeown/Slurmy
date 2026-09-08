# 🧪 Slurmy prover examples

These examples build real theorem provers in Slurm jobs, download the resulting
cluster-native binaries, generate readable Slurmy submission files, and provide
Makefile targets for submitting and monitoring experiments.

For installation, custom solver descriptions, and the full command reference,
see the [main Slurmy documentation](../README.md).

<details>
<summary><strong>📚 Available examples</strong></summary>

| Directory | What `make` obtains and builds |
| --- | --- |
| [`example-vampire`](example-vampire/) | The latest Vampire source from its [official Git repository](https://github.com/vprover/vampire), including its submodules; built as a CMake release binary |
| [`example-e`](example-e/) | The latest E source from its [official Git repository](https://github.com/eprover/eprover); configured and rebuilt as a first-order prover |
| [`example-drodi`](example-drodi/) | Drodi 4.1.1 from its [official CASC-J13 source archive](https://tptp.org/CASC/J13/SystemSources/Drodi---4.1.1.tgz); the archive is checked against the SHA-256 recorded in its build recipe |
| [`example-combined`](example-combined/) | All three provers in one submission, run on five easy and five hard problems with a three-second CPU limit per call |
| [`example-template`](example-template/) | A documented, reusable Makefile workflow whose variables cover remote building, solver limits, problem globs, array layout, submission, monitoring, and synchronization |
| [`example-maker`](example-maker/) | A one-question-at-a-time Textual application that validates existing solver and problem paths, then creates an `example-NAME/` workflow without copying those inputs |

</details>

<details>
<summary><strong>🧭 Create your own workflow</strong></summary>

<blockquote>

For a guided setup, launch the interactive maker:

```bash
cd examples/example-maker
make
```

The maker supplies no defaults and asks one question at a time. It validates
each solver-description path and problem glob before advancing, then asks for
the per-call limits, Slurm resource requests, and array layout. The final review
shows every resolved input location before **Create workflow** writes anything.
It creates a sibling `example-NAME/` directory without overwriting existing
work.

The solver descriptions, solver roots, and problems remain in their existing
locations; they do not need to be below `examples/` or the maker directory.
The generated workflow records absolute paths, and the normal `submit.sh`
packaging transfers those inputs when the experiment is submitted.

If you prefer to work directly, copy `example-template/` to a new
`example-NAME/` directory. Its Makefile begins with one documented variable
block. Edit that block together with `build.sh` and `solver.solver`, add files
under `problems/`, then use the same commands as every included example:

```bash
make
make submit
make monitor   # Or: make sync
```

Generated workflows keep their choices in the short `workflow.mk` file and
reuse the template Makefile unchanged. This separates experiment settings from
the common packaging, submission, monitoring, and synchronization logic.

</blockquote>

</details>

<details>
<summary><strong>🚀 Single-prover workflow</strong></summary>

<blockquote>

Each single-prover example contains the same three real TPTP problems and a
Makefile and remote build recipe. The examples do not contain checked-in prover
binaries or source trees. On the first run, `make` submits Slurm build jobs for
the prover and `runsolver`, downloads their artifacts, and calls `slurmy.py` to
create `submit.sh` and `submit.sh.files/`.

From the repository root, build, inspect, and submit Vampire with:

```bash
cd examples/example-vampire
make
less submit.sh
less submit.sh.files/slurm_job.sh
less submit.sh.files/batches/batch_000000.sh
make submit
make monitor
```

Use `examples/example-e` or `examples/example-drodi` in the first command to
run the corresponding example.

<details>
<summary><strong>🛠️ Makefile targets</strong></summary>

The single-prover Makefiles provide the same targets:

| Command | Effect |
| --- | --- |
| `make` or `make all` | Build the prover and `runsolver` on a compute node if needed, download them, then generate the submission files |
| `make build` | Run the remote builds without generating a submission |
| `make submit` | Run the generated `submit.sh` and return after Slurm accepts the job array |
| `make monitor` | Open the Slurmy dashboard for `datalab` |
| `make clean` | Remove only `submit.sh` and `submit.sh.files/` |
| `make distclean` | Also remove old local source data and the downloaded prover binary |

</details>

<details>
<summary><strong>✅ Expected results</strong></summary>

Connect to the TU Wien VPN before building or submitting if `datalab` is not
reachable directly. Each single-prover example submits two array elements to
`CPU-amd`. Vampire and E should quickly report the following expected statuses;
Drodi also proves the first two within the example limit but may time out on
the satisfiable problem:

```text
PUZ001+1.p  Theorem
ALG002-1.p  Unsatisfiable
ALG299-1.p  Satisfiable
```

</details>

</blockquote>

</details>

<details>
<summary><strong>🔬 Combined example</strong></summary>

The combined example exercises multiple solver descriptions in one Slurmy job.
Its `problems/easy/` directory contains five low-rated TPTP problems, while
`problems/hard/` contains five problems taken from the locally curated hard
benchmark set. All ten are self-contained FOF or CNF files with no external
axiom includes.

```text
easy: ALG002-1, PUZ001+1, PUZ002-1, PUZ003-1, PUZ004-1
hard: MPT0554+1, MPT1048+1, MPT1388+1, MPT1787+1, MPT1887+1
```

Running `make` builds the three single-prover examples and generates 30 calls:
each of Vampire, E, and Drodi on every problem. The calls are divided among six
Slurm array tasks, with a three-second CPU limit and six-second wall-clock
limit per prover call.

From the repository root:

```bash
cd examples/example-combined
make
make submit
make sync
```

`make sync` follows the latest remote Slurmy job until it completes and stores
the incrementally downloaded results under `slurmy-results/` at the repository
root. To avoid relying on which remote job is newest, pass the Slurmy ID printed
by `make submit` explicitly:

```bash
make sync SLURMY_ID=alice_1786464000_12345
```

Use `make monitor` instead when you want to watch the same run interactively.
`make distclean` removes the generated submission and downloaded binaries
shared with the three single-prover examples.

All Makefile targets use `datalab` by default. Override it consistently with
the environment or a Make variable, for example `SLURMY_HOST=my-cluster make`.

The expected three-second test pattern is that every prover solves the easy
set, while the hard set produces a mixture of solutions and ordinary time
limits. Exact hard-problem results can change when the Makefiles fetch newer
Vampire or E revisions.

</details>
