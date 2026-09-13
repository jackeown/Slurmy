#!/usr/bin/env bash
# This recipe runs in a Slurm job on a compute node, not on your laptop.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

# Replace this illustrative recipe with the commands that build your solver.
# Download or clone sources into SLURMY_BUILD_WORK, then copy the declared
# SOLVER_ARTIFACT into SLURMY_BUILD_OUTPUT with executable permissions.
git clone --depth 1 https://github.com/OWNER/PROJECT.git \
    "$SLURMY_BUILD_WORK/source"
make -C "$SLURMY_BUILD_WORK/source" -j"${SLURM_CPUS_PER_TASK:-1}"
install -m 0755 -- "$SLURMY_BUILD_WORK/source/my-solver" \
    "$SLURMY_BUILD_OUTPUT/my-solver"
