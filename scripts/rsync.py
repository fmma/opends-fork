#!/usr/bin/env python3
"""Rsync the local working tree to the target."""

import subprocess
import sys
import tomllib
from pathlib import Path

from _helpers import fail

root = Path(__file__).resolve().parent.parent

with open(root / "configs" / "transport.toml", "rb") as f:
    transport = tomllib.load(f)

with open(root / "configs" / "test.toml", "rb") as f:
    test = tomllib.load(f)

ssh = transport["cijoe"]["transport"]["ssh"]
dest = test["test"]["repo_path"]
target = f"{ssh['username']}@{ssh['hostname']}"

rc = subprocess.run(
    ["rsync", "-az", "--delete",
     "--filter=:- .gitignore",
     "--exclude=.git/",
     "--exclude=cijoe-output/",
     "--exclude=cijoe-archive/",
     f"{root}/", f"{target}:{dest}/"],
).returncode

if rc == 0:
    print(f"ok: synced to {target}:{dest}")
else:
    fail(f"rsync (rc={rc})")
sys.exit(rc)
