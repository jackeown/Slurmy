#!/usr/bin/env bash
set -euo pipefail
umask 077
[[ "$1" =~ ^[A-Za-z0-9._-]+$ ]] || exit 2
install -d -m 700 "$HOME/slurmy" "$HOME/slurmy/jobs"
JOB_DIR="$HOME/slurmy/jobs/$1"
STAGING="$HOME/slurmy/jobs/.staging-$1"
if [[ -d "$STAGING/incoming" && ! -L "$STAGING" && ! -e "$JOB_DIR" ]]; then
    exit 0  # Safe retry after an SSH acknowledgment was lost.
fi
[[ ! -e "$JOB_DIR" && ! -e "$STAGING" ]] || { echo "Job or upload staging directory already exists: $JOB_DIR" >&2; exit 1; }
mkdir -m 700 -p "$STAGING/incoming"
