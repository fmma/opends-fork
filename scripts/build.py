#!/usr/bin/env python3
"""Build OpenDS on the target.

Assumes the tree has been synced to the target (rsync.py).
"""

import subprocess
import sys
from pathlib import Path

from _helpers import fail

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/deps.toml",
           "-c", "configs/test.toml"]
rc = subprocess.run(
    ["cijoe", "-m", "-s", "-o", "cijoe-output-build", *configs,
     "tasks/setup_opends.yaml"],
    cwd=root,
).returncode
if rc:
    fail(f"build (rc={rc})")
sys.exit(rc)
