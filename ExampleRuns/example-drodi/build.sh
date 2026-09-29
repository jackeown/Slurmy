#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

gcc -O3 -DNDEBUG -std=gnu11 -o "$SLURMY_BUILD_OUTPUT/drodi" \
    "$SLURMY_BUILD_WORK"/*.c -lm -pthread
