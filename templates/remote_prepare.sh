#!/usr/bin/env bash
set -euo pipefail
[[ "$1" =~ ^[A-Za-z0-9._-]+$ ]] || exit 2
JOB_DIR="$HOME/Slurmy/$1"
[[ ! -e "$JOB_DIR" ]] || { echo "Already exists: $JOB_DIR" >&2; exit 1; }
mkdir -p "$JOB_DIR/incoming"
