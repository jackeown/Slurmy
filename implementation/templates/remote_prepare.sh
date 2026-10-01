#!/usr/bin/env bash
set -euo pipefail
umask 077
[[ "$1" =~ ^[A-Za-z0-9._-]+$ ]] || exit 2
install -d -m 700 "$HOME/slurmy" "$HOME/slurmy/jobs"
JOB_DIR="$HOME/slurmy/jobs/$1"
[[ ! -e "$JOB_DIR" ]] || { echo "Already exists: $JOB_DIR" >&2; exit 1; }
mkdir -m 700 -p "$JOB_DIR/incoming"
