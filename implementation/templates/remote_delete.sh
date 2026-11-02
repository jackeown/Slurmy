#!/usr/bin/env bash
# Called only after remote_stop.sh has completed; recheck under controller lock.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -e "$job" || -L "$job" ]] || { echo 'Job was already deleted.'; exit 0; }
# Keep the deletion lock outside the directory being removed. Retried requests
# must wait for the original rm instead of opening a replacement lock inode.
locks="$HOME/slurmy/.delete-locks"
[[ ! -L "$locks" ]] || exit 2
mkdir -p -m 700 -- "$locks"
[[ ! -L "$locks/$1.lock" && ! -L "$locks/$1.deleting" ]] || exit 2
exec 8> "$locks/$1.lock"
flock -w 60 8 || { echo 'Deletion is still in progress; waiting to retry.' >&2; exit 75; }
[[ -e "$job" || -L "$job" ]] || { echo 'Job was already deleted.'; exit 0; }
[[ -d "$job" && ! -L "$job" && ( -f "$job/.stopped" || -f "$locks/$1.deleting" ) ]] || { echo 'Stopped job directory not found.' >&2; exit 2; }
exec 9> "$job/.submit-controller.lock"
flock -w 60 9 || { echo 'Submission controller did not stop; job was kept.' >&2; exit 3; }
snapshot=$(squeue -h -r -u "$USER" -o '%Z') || { echo 'Cannot confirm SLURM queue state; job was kept.' >&2; exit 3; }
if grep -Fxq -- "$job" <<< "$snapshot"; then
    echo 'SLURM allocations reappeared; job was kept. Retry Delete.' >&2
    exit 3
fi
: > "$locks/$1.deleting"
rm -rf -- "$job"
rm -f -- "$locks/$1.deleting"
echo 'Cluster job files deleted.'
