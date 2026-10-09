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
SSH_OPTIONS=(-T -o BatchMode=yes -o ConnectTimeout=10)
ssh_stage() {
    local script=$1 label=$2 attempt
    for attempt in 1 2 3; do
        if ssh "${SSH_OPTIONS[@]}" -- "$HOST" bash -s -- "$JOB_ID" < "$ASSETS/$script"; then return 0; fi
        echo "$label: SSH attempt $attempt/3 failed." >&2
        (( attempt == 3 )) || sleep 2
    done
    return 1
}
: "${SLURMY_PARTITION:?Set SLURMY_PARTITION to the Slurm partition to use}"
[[ "$SLURMY_PARTITION" =~ ^[A-Za-z0-9_.-]+$ ]] || { echo 'Invalid partition' >&2; exit 1; }
JOB_NAME_B64=$(printf '%s' "$JOB_DISPLAY_NAME" | base64 | tr -d '\r\n')
if [[ -n ${SLURMY_RESUME_JOB_ID:-} ]]; then
    JOB_ID=$SLURMY_RESUME_JOB_ID
    [[ "$JOB_ID" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid SLURMY_RESUME_JOB_ID.' >&2; exit 1; }
    echo "Resuming accepted batches for $JOB_ID; no inputs are rebuilt or uploaded."
    ssh "${SSH_OPTIONS[@]}" -- "$HOST" bash -s -- "$JOB_ID" "$SLURMY_PARTITION" "${SLURMY_MAX_OUTSTANDING:-32}" "$JOB_PREFIX" "$JOB_NAME_B64" < "$ASSETS/remote_submit.sh"
    exit $?
fi

# 1. Build each non-empty building.txt recipe remotely and fetch its outputs.
echo 'Submission phase: Building dependencies on cluster compute nodes'
bash "$ASSETS/builds/run.sh"

# 2. Package declared resources and problems with their local path layout.
echo 'Submission phase: Packaging inputs'
# The local helper streams tar through gzip and reports bytes as they are packed.
TEMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/slurmy-submit.XXXXXXXX")
trap 'rm -rf -- "$TEMP_DIR"' EXIT
python "$ASSETS/package_inputs.py" --label inputs --output "$TEMP_DIR/inputs.tar.gz" --base / --paths "$ASSETS/archive-paths.txt"
python "$ASSETS/package_inputs.py" --label job-files --output "$TEMP_DIR/job-files.tar.gz" --base "$ASSETS"
JOB_ID="${JOB_PREFIX}_$(date +%s)_$$"

# 3. Transfer archives. Remote shell logic lives in readable helper files.
echo 'Submission phase: Uploading inputs to cluster'
if ! ssh_stage remote_prepare.sh 'Preparing upload'; then
    echo 'SSH failed while creating upload staging; no Slurm batch was submitted.' >&2; exit 1
fi
upload_ok=false
for attempt in 1 2 3; do
    if scp -o BatchMode=yes -o ConnectTimeout=10 -- "$TEMP_DIR/inputs.tar.gz" "$TEMP_DIR/job-files.tar.gz" "$HOST:slurmy/jobs/.staging-$JOB_ID/incoming/"; then
        upload_ok=true; break
    fi
    echo "Uploading inputs: SSH attempt $attempt/3 failed." >&2
    (( attempt == 3 )) || sleep 2
done
if [[ "$upload_ok" != true ]]; then
    echo 'Upload failed; the incomplete staging directory was not published or submitted.' >&2; exit 1
fi
if ! ssh_stage remote_finish_upload.sh 'Committing upload'; then
    echo 'SSH failed while committing the upload. Check the cluster before retrying; no Slurm batch was submitted by this command.' >&2; exit 1
fi

# 4. Calculate allocations using cluster topology, then submit each batch.
echo 'Submission phase: Submitting Slurm batches'
if ! ssh "${SSH_OPTIONS[@]}" -- "$HOST" bash -s -- "$JOB_ID" "$SLURMY_PARTITION" "${SLURMY_MAX_OUTSTANDING:-32}" "$JOB_PREFIX" "$JOB_NAME_B64" < "$ASSETS/remote_submit.sh"; then
    echo "Submission connection failed. Some batches may already be accepted; inspect job $JOB_ID before retrying." >&2
    echo "To resume only unaccepted batches: SLURMY_RESUME_JOB_ID=$JOB_ID bash ./submit.sh" >&2
    exit 1
fi
