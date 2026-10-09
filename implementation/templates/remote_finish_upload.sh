#!/usr/bin/env bash
# Validate and unpack a complete upload before exposing it as a Slurmy job.
set -euo pipefail
umask 077
[[ "${1:-}" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid job ID.' >&2; exit 2; }
STAGING="$HOME/slurmy/jobs/.staging-$1"
JOB_DIR="$HOME/slurmy/jobs/$1"
if [[ -f "$JOB_DIR/.inputs-ready" && ! -L "$JOB_DIR" ]]; then
    printf 'Upload for %s was already committed.\n' "$1"
    exit 0  # Safe retry after an SSH acknowledgment was lost.
fi
[[ -d "$STAGING" && ! -L "$STAGING" && ! -e "$JOB_DIR" ]] || {
    echo 'Upload staging is missing or the job already exists.' >&2; exit 1;
}
[[ -s "$STAGING/incoming/inputs.tar.gz" && -s "$STAGING/incoming/job-files.tar.gz" ]] || {
    echo 'Upload is incomplete; no job was published.' >&2; exit 1;
}
mkdir -m 700 -p "$STAGING/rootfs" "$STAGING/results/index" "$STAGING/progress" "$STAGING/logs"
tar -xzf "$STAGING/incoming/job-files.tar.gz" -C "$STAGING"
tar -xzf "$STAGING/incoming/inputs.tar.gz" -C "$STAGING/rootfs"
[[ -f "$STAGING/metadata.json" ]] || { echo 'Uploaded job metadata is missing.' >&2; exit 1; }
: > "$STAGING/results/index/.ready"
: > "$STAGING/.inputs-ready"
mv -T -- "$STAGING" "$JOB_DIR"
printf 'Uploaded and published %s atomically.\n' "$1"
