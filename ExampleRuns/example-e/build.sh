#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

BUILD_TMP=$(mktemp -d "${TMPDIR:-/tmp}/slurmy-e-build.XXXXXXXX")
trap 'rm -rf -- "$BUILD_TMP"' EXIT

git clone --depth 1 https://github.com/eprover/eprover.git \
    "$BUILD_TMP/source"
cd -- "$BUILD_TMP/source"
./configure
make -j"${SLURM_CPUS_PER_TASK:-1}" rebuild
install -m 0755 -- PROVER/eprover "$SLURMY_BUILD_OUTPUT/eprover"
