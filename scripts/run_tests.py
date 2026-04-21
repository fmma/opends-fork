#!/usr/bin/env python3
"""Run all test suites on the target."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/test.toml"]

sys.exit(subprocess.run(
    ["cijoe", "-m", "-s", "-o", "cijoe-output-test", *configs, "tasks/test.yaml"],
    cwd=root,
).returncode)
