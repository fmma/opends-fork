#!/usr/bin/env python3
"""Install OpenDS target dependencies (xNVMe) on the target.

Required only when the aisio backend is needed. Assumes the tree has
been synced to the target (rsync.py).
"""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/test.toml"]
sys.exit(subprocess.run(
    ["cijoe", "-m", "-s", "-o", "cijoe-output-setup-deps", *configs,
     "tasks/setup_xnvme.yaml"],
    cwd=root,
).returncode)
