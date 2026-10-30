#!/usr/bin/env bash
# Continue only unfinished calls in the same Slurmy job.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -d "$job" && ! -L "$job" && -f "$job/.stopped" ]] || { echo 'Stop the job before resuming it.' >&2; exit 2; }
[[ -f "$job/submission_context.sh" && -f "$job/remote_submit.sh" ]] || { echo 'This job predates resumable submissions.' >&2; exit 2; }
snapshot=$(squeue -h -r -u "$USER" -o '%Z') || { echo 'Cannot confirm SLURM queue state.' >&2; exit 1; }
if grep -Fxq -- "$job" <<< "$snapshot"; then
    echo 'SLURM still has active allocations for this job.' >&2; exit 1
fi
flock -n "$job/.submit-controller.lock" true || { echo 'The submission controller is still active.' >&2; exit 1; }
cd "$job"
source ./submission_context.sh
rm -- .stopped
if [[ -f ./remote_dispatch.sh ]]; then
    dispatcher=./remote_dispatch.sh
else
    # Compatibility: older jobs have no detached dispatcher in their bundle.
    # Run the same launcher sent with this request, using the saved controller.
    dispatcher=$(mktemp "$job/.resume-dispatch.XXXXXXXX")
    trap 'rm -f -- "$dispatcher"' EXIT
    cat > "$dispatcher" <<'SLURMY_DISPATCH'
#!/usr/bin/env bash
# Start the submission controller independently of the client/SSH lifetime.
set -euo pipefail
[[ "${1:-}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || exit 2
job="$HOME/slurmy/jobs/$1"
[[ -d "$job" && ! -L "$job" && -f "$job/remote_submit.sh" ]] || exit 2
cd "$job"
exec 8> .dispatch-launch.lock
flock 8
if [[ -f .submit-controller.lock ]] && ! flock -n .submit-controller.lock true; then
    echo "Slurmy job ID: $1"
    echo 'Submission controller is already active; keeping it.'
    exit 0
fi
[[ ! -f .stopped ]] || { echo 'Job is stopped. Use Resume to continue.' >&2; exit 1; }
accepted=0; total=0
if [[ -f submission.state ]]; then read -r _ accepted total < submission.state || true; fi
printf 'submitting %s %s\n' "${accepted:-0}" "${total:-0}" > submission.state.launch
mv -f submission.state.launch submission.state
# Separate stdin, stdout, stderr and the launch lock from the SSH session.
nohup bash ./remote_submit.sh "$@" </dev/null >> submission-controller.log 2>&1 8>&- &
controller=$!
sleep 0.2
if ! kill -0 "$controller" 2>/dev/null; then
    if wait "$controller"; then :; else
        tail -n 30 submission-controller.log >&2
        exit 1
    fi
fi
echo "Slurmy job ID: $1"
echo 'Submission phase: Cluster submission controller started; it continues if this connection closes'
SLURMY_DISPATCH
fi
bash "$dispatcher" "$1" "$SAVED_PARTITION" "$SAVED_MAX_OUTSTANDING" "$SAVED_JOB_PREFIX" "$SAVED_JOB_NAME_B64" 1 "${SAVED_RESERVATION:--}"
