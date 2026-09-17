#!/usr/bin/env bash
# Run locally. The three input files have already been compiled into
# submit.sh.files/. Builds run on compute nodes and their outputs are downloaded
# before packaging. Experiment submission returns without waiting for results.
# Set SLURMY_HOST (default datalab) and SLURMY_PARTITION in your environment.
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ASSETS="$HERE/submit.sh.files"
HOST=${SLURMY_HOST:-datalab}
: "${SLURMY_PARTITION:?Set SLURMY_PARTITION to the Slurm partition to use}"
[[ "$SLURMY_PARTITION" =~ ^[A-Za-z0-9_.-]+$ ]] || { echo 'Invalid partition' >&2; exit 1; }

# 1. Build each non-empty building.txt recipe remotely and fetch its outputs.
bash "$ASSETS/builds/run.sh"

# 2. Package declared resources and problems with their local path layout.
# Both BSD tar on macOS and GNU tar support these options.
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/slurmy-submit.XXXXXXXX")
trap 'rm -rf -- "$TEMP_DIR"' EXIT
COPYFILE_DISABLE=1 tar -chzf "$TEMP_DIR/inputs.tar.gz" -C / --null -T "$ASSETS/archive-paths.txt"
COPYFILE_DISABLE=1 tar -czf "$TEMP_DIR/job-files.tar.gz" -C "$ASSETS" .
REMOTE_USER=$(ssh -T -G -- "$HOST" | awk 'tolower($1)=="user" && !seen++ {print $2}')
[[ "$REMOTE_USER" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid SSH username' >&2; exit 1; }
JOB_ID="${REMOTE_USER}_$(date +%s)_$$"

# 3. Transfer archives. Remote shell logic lives in readable helper files.
ssh -T -- "$HOST" bash -s -- "$JOB_ID" < "$ASSETS/remote_prepare.sh"
scp -- "$TEMP_DIR/inputs.tar.gz" "$TEMP_DIR/job-files.tar.gz" "$HOST:Slurmy/$JOB_ID/incoming/"

# 4. Calculate allocations using cluster topology, then submit each batch.
ssh -T -- "$HOST" bash -s -- "$JOB_ID" "$SLURMY_PARTITION" < "$ASSETS/remote_submit.sh"
