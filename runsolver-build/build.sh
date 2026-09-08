#!/usr/bin/env bash
# Remote build recipe used by slurmy-build.py.
set -euo pipefail

: "${SLURMY_BUILD_WORK:?slurmy-build.py must set SLURMY_BUILD_WORK}"
: "${SLURMY_BUILD_OUTPUT:?slurmy-build.py must set SLURMY_BUILD_OUTPUT}"

VERSION=3.4.1
SHA256=6fb8c8c849e09593b509a9df1aaddb94b8187f65bb217ff707c8252fddd79e2f
URL="https://www.cril.univ-artois.fr/~roussel/runsolver/runsolver-${VERSION}.tar.bz2"
FALLBACK_ADDRESS=193.49.115.72
ARCHIVE="$SLURMY_BUILD_WORK/runsolver.tar.bz2"

for command in curl sha256sum tar make g++ install; do
    command -v "$command" >/dev/null || {
        echo "Missing build command: $command" >&2
        exit 1
    }
done

echo "Downloading runsolver $VERSION..."
if ! curl --fail --location --retry 1 --output "$ARCHIVE" "$URL"; then
    echo "Normal DNS failed; retrying the verified source via its current address..."
    curl --fail --location \
        --resolve "www.cril.univ-artois.fr:443:$FALLBACK_ADDRESS" \
        --output "$ARCHIVE" "$URL"
fi
printf '%s  %s\n' "$SHA256" "$ARCHIVE" | sha256sum --check --status
tar -xjf "$ARCHIVE" -C "$SLURMY_BUILD_WORK"

# The non-NUMA form avoids dependence on a cluster-specific libnuma installation.
make -C "$SLURMY_BUILD_WORK/runsolver/src" clean
make -C "$SLURMY_BUILD_WORK/runsolver/src" -j"${SLURM_CPUS_PER_TASK:-1}" \
    CFLAGS="-std=c++11 -Dtmpdebug -Wall -DVERSION=\\\"$VERSION\\\" -DSVNVERSION=\\\"4412\\\" -DWSIZE=64" \
    LDFLAGS="-Wl,--build-id" \
    LIBS= \
    runsolver

install -m 0755 -- "$SLURMY_BUILD_WORK/runsolver/src/runsolver" \
    "$SLURMY_BUILD_OUTPUT/runsolver"
runner_help=$("$SLURMY_BUILD_OUTPUT/runsolver" 2>&1 || true)
grep -q -- '--rss-swap-limit' <<< "$runner_help" || {
    echo "Built runsolver failed its feature check." >&2
    exit 1
}
