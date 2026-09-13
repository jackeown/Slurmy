#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

VERSION=4.1.1
URL="https://tptp.org/CASC/J13/SystemSources/Drodi---${VERSION}.tgz"
SHA256=4967b522235df03e2b3e5a35e5284aa742cbfd6875b0f0a0c904ee8e0d3220ce
ARCHIVE="$SLURMY_BUILD_WORK/drodi.tgz"
SOURCE="$SLURMY_BUILD_WORK/source"

curl --fail --location --retry 2 --output "$ARCHIVE" "$URL"
printf '%s  %s\n' "$SHA256" "$ARCHIVE" | sha256sum --check --status
mkdir -p -- "$SOURCE"
tar -xzf "$ARCHIVE" --strip-components=1 -C "$SOURCE"
gcc -O3 -DNDEBUG -std=gnu11 -o "$SLURMY_BUILD_OUTPUT/drodi" \
    "$SOURCE"/*.c -lm -pthread
