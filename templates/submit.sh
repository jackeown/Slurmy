#!/usr/bin/env bash
# Run locally. The three input files have already been compiled into
# submit.sh.files/. Builds run on compute nodes and their outputs are downloaded
# before packaging. Job submission returns without waiting for results.
# Set SLURMY_HOST (default datalab) and SLURMY_PARTITION in your environment.
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ASSETS="$HERE/submit.sh.files"
source "$ASSETS/workflow_name.sh"
HOST=${SLURMY_HOST:-datalab}
: "${SLURMY_PARTITION:?Set SLURMY_PARTITION to the Slurm partition to use}"
[[ "$SLURMY_PARTITION" =~ ^[A-Za-z0-9_.-]+$ ]] || { echo 'Invalid partition' >&2; exit 1; }

# 1. Build each non-empty building.txt recipe remotely and fetch its outputs.
echo 'Submission phase: Building dependencies on cluster compute nodes'
bash "$ASSETS/builds/run.sh"

# 2. Package declared resources and problems with their local path layout.
echo 'Submission phase: Packaging inputs'
# Both BSD tar on macOS and GNU tar support these options.
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/slurmy-submit.XXXXXXXX")
trap 'rm -rf -- "$TEMP_DIR"' EXIT
COPYFILE_DISABLE=1 tar -chzf "$TEMP_DIR/inputs.tar.gz" -C / --null -T "$ASSETS/archive-paths.txt"
COPYFILE_DISABLE=1 tar -czf "$TEMP_DIR/job-files.tar.gz" -C "$ASSETS" .
JOB_ID="${JOB_PREFIX}_$(date +%s)_$$"
JOB_NAME_B64=$(printf '%s' "$JOB_DISPLAY_NAME" | base64 | tr -d '\r\n')

# 3. Transfer archives. Remote shell logic lives in readable helper files.
echo 'Submission phase: Uploading inputs to cluster'
ssh -T -- "$HOST" bash -s -- "$JOB_ID" < "$ASSETS/remote_prepare.sh"
scp -- "$TEMP_DIR/inputs.tar.gz" "$TEMP_DIR/job-files.tar.gz" "$HOST:Slurmy/$JOB_ID/incoming/"

# 4. Calculate allocations using cluster topology, then submit each batch.
echo 'Submission phase: Submitting Slurm batches'
ssh -T -- "$HOST" bash -s -- "$JOB_ID" "$SLURMY_PARTITION" "${SLURMY_MAX_OUTSTANDING:-32}" "$JOB_PREFIX" "$JOB_NAME_B64" < "$ASSETS/remote_submit.sh"
