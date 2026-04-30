#!/usr/bin/env python3
"""Install OpenDS target dependencies (xNVMe, xal, fil) on the target.

xNVMe is required for the aisio backend; xal and fil are required for
the filperf-driven benchmark suite. Assumes the tree has been synced
to the target (rsync.py).
"""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/deps.toml",
           "-c", "configs/test.toml"]
tasks = [
    "tasks/setup_xnvme.yaml",
    "tasks/setup_xal.yaml",
    "tasks/setup_fil.yaml",
]
for task in tasks:
    rc = subprocess.run(
        ["cijoe", "-m", "-s", "-o", "cijoe-output-setup-deps", *configs, task],
        cwd=root,
    ).returncode
    if rc:
        sys.exit(rc)
