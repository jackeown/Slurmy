#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

BUILD_TMP=$(mktemp -d "${TMPDIR:-/tmp}/slurmy-vampire-build.XXXXXXXX")
trap 'rm -rf -- "$BUILD_TMP"' EXIT

git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/vprover/vampire.git "$BUILD_TMP/source"
cmake -S "$BUILD_TMP/source" -B "$BUILD_TMP/source/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build "$BUILD_TMP/source/build" \
    --parallel "${SLURM_CPUS_PER_TASK:-1}"
install -m 0755 -- "$BUILD_TMP/source/build/vampire" \
    "$SLURMY_BUILD_OUTPUT/vampire"
