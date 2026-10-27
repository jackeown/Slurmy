#!/usr/bin/env bash
# Called only after remote_stop.sh has completed; recheck under controller lock.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -d "$job" && ! -L "$job" && -f "$job/.stopped" ]] || { echo 'Stopped job directory not found.' >&2; exit 2; }
exec 9> "$job/.submit-controller.lock"
flock -w 60 9 || { echo 'Submission controller did not stop; job was kept.' >&2; exit 3; }
snapshot=$(squeue -h -r -u "$USER" -o '%Z') || { echo 'Cannot confirm SLURM queue state; job was kept.' >&2; exit 3; }
if grep -Fxq -- "$job" <<< "$snapshot"; then
    echo 'SLURM allocations reappeared; job was kept. Retry Delete.' >&2
    exit 3
fi
rm -r -- "$job"
echo 'Cluster job files deleted.'
