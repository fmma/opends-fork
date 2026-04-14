#!/usr/bin/env python3
"""Run all test suites on the target."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/test.toml"]

tasks = [
    "tasks/test_ref.yaml",
    "tasks/test_gds.yaml",
]

for task in tasks:
    name = Path(task).stem
    rc = subprocess.run(
        ["cijoe", "-m", "-s", "-o", f"cijoe-output-{name}", *configs, task],
        cwd=root,
    ).returncode
    if rc:
        sys.exit(rc)
