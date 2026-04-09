#!/usr/bin/env python3
"""Run the device setup task on the target."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent

configs = ["-c", "configs/transport.toml", "-c", "configs/test.toml"]
sys.exit(subprocess.run(
    ["cijoe", "-m", "-s", *configs, "tasks/setup_device.yaml"],
    cwd=root,
).returncode)
