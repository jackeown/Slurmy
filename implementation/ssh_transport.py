"""Reuse authenticated SSH transports without changing the user's SSH config."""
from __future__ import annotations
import os
from pathlib import Path
import stat
import tempfile


def connection_options() -> list[str]:
    directory=Path(tempfile.gettempdir())/f'slurmy-ssh-{os.getuid()}'
    if len(str(directory))+41 >= 104:
        return []
    try:
        directory.mkdir(mode=0o700,exist_ok=True)
        info=directory.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid!=os.getuid() or stat.S_IMODE(info.st_mode)!=0o700:
            return []
    except OSError:
        return []
    # %C includes the destination, port, user and jump host; unrelated aliases
    # cannot accidentally share a connection. Expire idle masters automatically.
    return ['-o','ServerAliveInterval=10','-o','ServerAliveCountMax=2','-o','ControlMaster=auto','-o','ControlPersist=60','-o',f'ControlPath={directory}/%C']


if __name__=='__main__':
    print('\n'.join(connection_options()))
