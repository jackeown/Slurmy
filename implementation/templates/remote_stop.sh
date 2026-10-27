#!/usr/bin/env bash
# Stop one Slurmy job without touching another job's SLURM allocations.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -d "$job" && ! -L "$job" ]] || { echo 'Job directory not found.' >&2; exit 2; }
: > "$job/.stopped"
for attempt in {1..30}; do
    # A concurrently starting resume may have cleared the marker after our
    # initial request. Keep stopping submission until its controller exits.
    : > "$job/.stopped"
    snapshot=$(squeue -h -r -u "$USER" -o '%i|%Z') || { echo 'Cannot confirm SLURM queue state; job remains marked stopped.' >&2; exit 1; }
    ids=()
    if [[ -n "$snapshot" ]]; then
        mapfile -t ids < <(awk -F'|' -v directory="$job" '$2==directory {print $1}' <<< "$snapshot")
    fi
    if (( ${#ids[@]} )); then scancel "${ids[@]}"; fi
    if (( ${#ids[@]} == 0 )) && flock -n "$job/.submit-controller.lock" true; then
        if [[ -f "$job/submission.state" ]]; then read -r _ accepted total < "$job/submission.state" || true; fi
        printf 'stopped %s %s\n' "${accepted:-0}" "${total:-0}" > "$job/submission.state"
        echo 'Job stopped: no active SLURM allocations or submission controller remain.'
        exit 0
    fi
    echo "Waiting for SLURM tasks and submission controller to stop (check $attempt/30)…"
    sleep 2
done
echo 'Stop requested, but SLURM or the submission controller is still active; check again before deletion.' >&2
exit 1
