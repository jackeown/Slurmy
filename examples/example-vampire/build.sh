#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/vprover/vampire.git "$SLURMY_BUILD_WORK/source"
cmake -S "$SLURMY_BUILD_WORK/source" -B "$SLURMY_BUILD_WORK/source/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build "$SLURMY_BUILD_WORK/source/build" \
    --parallel "${SLURM_CPUS_PER_TASK:-1}"
install -m 0755 -- "$SLURMY_BUILD_WORK/source/build/vampire" \
    "$SLURMY_BUILD_OUTPUT/vampire"
