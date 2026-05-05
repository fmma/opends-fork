#!/usr/bin/env python3
"""Run all test suites on the target."""

import subprocess
import sys
from pathlib import Path

from _helpers import fail

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/deps.toml",
           "-c", "configs/test.toml"]

rc = subprocess.run(
    ["cijoe", "-m", "-s", "-o", "cijoe-output-test", *configs, "tasks/test.yaml"],
    cwd=root,
).returncode
if rc:
    fail(f"run_tests (rc={rc})")
sys.exit(rc)
