#!/usr/bin/env bash
# Rename a finished Slurmy job on the cluster; called by the local web app.
set -euo pipefail
old_id=$1
new_id=$2
name_b64=$3
[[ "$old_id" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ && "$new_id" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] || {
    echo 'Invalid Slurmy job ID.' >&2; exit 2;
}
base=${SLURMY_JOBS_ROOT:-"$HOME/slurmy/jobs"}
source_dir="$base/$old_id"
target_dir="$base/$new_id"
[[ -d "$source_dir" && ! -L "$source_dir" ]] || { echo 'Job directory not found or is a symlink.' >&2; exit 2; }
if [[ "$source_dir" != "$target_dir" && ( -e "$target_dir" || -L "$target_dir" ) ]]; then
    echo "Destination job ID already exists: $new_id" >&2; exit 3
fi
queue=$(squeue -h -u "$USER" -o '%Z') || { echo 'Could not verify Slurm queue state.' >&2; exit 4; }
if grep -Fxq -- "$source_dir" <<< "$queue"; then
    echo 'This job still has a queued or running Slurm allocation. Rename it after the job finishes.' >&2
    exit 4
fi
if [[ "$source_dir" != "$target_dir" ]]; then mv -- "$source_dir" "$target_dir"; fi
if ! python3 - "$target_dir/metadata.json" "$old_id" "$new_id" "$name_b64" <<'PY'
import base64
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
old_id, new_id = sys.argv[2:4]
name = base64.b64decode(sys.argv[4], validate=True).decode('utf-8').strip()
if not name or len(name) > 100 or any(ord(char) < 32 or ord(char) == 127 for char in name):
    raise SystemExit('Invalid job name.')
metadata = json.loads(path.read_text(encoding='utf-8'))
history = metadata.get('previous_job_ids', [])
if not isinstance(history, list):
    history = []
if old_id != new_id and old_id not in history:
    history.append(old_id)
metadata['previous_job_ids'] = history
metadata['job_id'] = new_id
metadata['job_name'] = name
temporary = path.with_suffix('.tmp')
temporary.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
temporary.replace(path)
PY
then
    if [[ "$source_dir" != "$target_dir" ]]; then mv -- "$target_dir" "$source_dir"; fi
    exit 5
fi
printf '%s\n' "$new_id"
