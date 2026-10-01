# Solver source snapshots

These source trees are included so the example workflows can demonstrate
uploading **local source** to a Slurm build job. The examples build on the
cluster; no prover binaries are committed here.

| Directory | Source | Snapshot |
| --- | --- | --- |
| `vampire/` | https://github.com/vprover/vampire | `d1e247b5e04a69b3879ca02a67eac71feeff8cc3` (2026-09-03) |
| `e/` | https://github.com/eprover/eprover | `3b7afc70fe77d3118bb95144c4735ca45f5f31cf` (2026-09-02) |
| `drodi/` | https://tptp.org/CASC/J13/SystemSources/Drodi---4.1.1.tgz | Version 4.1.1, SHA-256 `4967b522235df03e2b3e5a35e5284aa742cbfd6875b0f0a0c904ee8e0d3220ce` |

The Vampire snapshot includes its CaDiCaL and VIRAS submodule source trees;
Z3 is optional and is not included. The upstream projects' notices and
licenses remain in their respective source directories. These are fixed
snapshots, not automatically updated to the latest upstream revision.
