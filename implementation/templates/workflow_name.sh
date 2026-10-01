#!/usr/bin/env bash
# Source from submit.sh.files/ or its builds/ subdirectory. Derive a safe,
# readable name from the workflow folder at execution time, even after rename.
WORKFLOW_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
WORKFLOW_NAME=${WORKFLOW_DIR##*/}
WORKFLOW_NAME=${WORKFLOW_NAME//[^a-zA-Z0-9_.-]/-}
while [[ -n "$WORKFLOW_NAME" && ! "$WORKFLOW_NAME" =~ ^[A-Za-z0-9] ]]; do
    WORKFLOW_NAME=${WORKFLOW_NAME:1}
done
WORKFLOW_NAME=${WORKFLOW_NAME:0:48}
WORKFLOW_NAME=${WORKFLOW_NAME:-workflow}
export WORKFLOW_NAME

# A submitted job may have its own name. Keep its display name in metadata,
# while using a filesystem- and Slurm-safe prefix for its ID and batch jobs.
JOB_DISPLAY_NAME=${SLURMY_JOB_NAME:-$WORKFLOW_NAME}
JOB_PREFIX=${JOB_DISPLAY_NAME//[^a-zA-Z0-9_.-]/-}
while [[ -n "$JOB_PREFIX" && ! "$JOB_PREFIX" =~ ^[A-Za-z0-9] ]]; do
    JOB_PREFIX=${JOB_PREFIX:1}
done
JOB_PREFIX=${JOB_PREFIX:0:48}
JOB_PREFIX=${JOB_PREFIX:-job}
export JOB_DISPLAY_NAME JOB_PREFIX
