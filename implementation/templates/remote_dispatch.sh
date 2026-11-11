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
: >> .submit-controller.lock
# Leave the SSH session/process group as well as its streams and launch lock.
# nohup alone ignores HUP but does not protect against group-directed TERM.
nohup setsid env -u BASH_ENV -u ENV bash --noprofile --norc ./remote_submit.sh "$@" </dev/null >> submission-controller.log 2>&1 8>&- &
controller=$!
sleep 0.2
if ! kill -0 "$controller" 2>/dev/null; then
    if wait "$controller"; then :; else
        printf 'failed %s %s\n' "${accepted:-0}" "${total:-0}" > submission.state.launch
        mv -f submission.state.launch submission.state
        tail -n 30 submission-controller.log >&2
        exit 1
    fi
fi
echo "Slurmy job ID: $1"
# Release the launch lock before waiting, so Stop and duplicate requests work.
flock -u 8
exec 8>&-
if [[ ${SLURMY_KEEP_SUBMISSION_SESSION:-1} == 1 ]]; then
    # setsid cannot escape logind's SSH session cgroup. With
    # KillUserProcesses=yes, logging out kills even nohup/setsid children.
    echo 'Submission phase: Keeping SSH connected while the controller submits batches'
    if wait "$controller"; then
        echo 'Submission phase: All batches accepted or submission stopped'
    else
        code=$?
        tail -n 30 submission-controller.log >&2
        exit "$code"
    fi
else
    echo 'Submission phase: Cluster submission controller started in detached mode'
fi
