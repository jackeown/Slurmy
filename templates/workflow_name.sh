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
