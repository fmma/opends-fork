#!/usr/bin/env python3
"""Sync local working tree to the target and build."""

import subprocess
import sys
import tomllib
from pathlib import Path

root = Path(__file__).resolve().parent.parent

with open(root / "configs" / "transport.toml", "rb") as f:
    transport = tomllib.load(f)

with open(root / "configs" / "test.toml", "rb") as f:
    test = tomllib.load(f)

ssh = transport["cijoe"]["transport"]["ssh"]
dest = test["opends"]["repository"]["path"]
target = f"{ssh['username']}@{ssh['hostname']}"

rc = subprocess.run(
    ["rsync", "-az", "--delete",
     "--filter=:- .gitignore",
     "--exclude=.git/",
     "--exclude=cijoe-output/",
     "--exclude=cijoe-archive/",
     f"{root}/", f"{target}:{dest}/"],
).returncode
if rc:
    sys.exit(rc)

configs = ["-c", "configs/transport.toml", "-c", "configs/test.toml"]
sys.exit(subprocess.run(
    ["cijoe", "-m", "-s", *configs, "tasks/setup_opends.yaml", "build"],
    cwd=root,
).returncode)
