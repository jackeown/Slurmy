# Prefix to remote_resume.sh for automatic recovery. Never cancel live work.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -d "$job" && ! -L "$job" ]] || exit 2
exec 7> "$job/.auto-recovery.lock"
flock -n 7 || { echo 'Another automatic recovery is active.'; exit 0; }
[[ ! -f "$job/.stopped" ]] || { echo 'Job was stopped; keeping it stopped.'; exit 0; }
snapshot=$(squeue -h -r -u "$USER" -o '%Z') || { echo 'Cannot confirm queue state; recovery deferred.' >&2; exit 1; }
if grep -Fxq -- "$job" <<< "$snapshot"; then
    echo 'SLURM tasks are active; no recovery needed.'; exit 0
fi
if ! flock -n "$job/.submit-controller.lock" true; then
    echo 'Submission controller is active; no recovery needed.'; exit 0
fi
echo 'Automatically resuming idle job; keeping completed results.'
: > "$job/.stopped"
