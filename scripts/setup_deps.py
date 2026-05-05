#!/usr/bin/env python3
"""Install OpenDS target dependencies on the target.

Builds and installs xNVMe, xal, OpenDS, and fil in dependency order.
xNVMe is required for the aisio backend; xal and fil are required
for the filperf-driven benchmark suite. fil's opends backend links
against installed libopends_aisio, so OpenDS is installed before fil
is built. Assumes the tree has been synced to the target (rsync.py).
"""

import subprocess
import sys
from pathlib import Path

from _helpers import fail

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/deps.toml",
           "-c", "configs/test.toml"]
tasks = [
    "tasks/setup_xnvme.yaml",
    "tasks/setup_xal.yaml",
    "tasks/setup_opends.yaml",
    "tasks/setup_fil.yaml",
]
for task in tasks:
    out = f"cijoe-output-{Path(task).stem}"
    rc = subprocess.run(
        ["cijoe", "-m", "-s", "-o", out, *configs, task],
        cwd=root,
    ).returncode
    if rc:
        fail(f"{task} (rc={rc})")
        sys.exit(rc)
