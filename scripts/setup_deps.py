#!/usr/bin/env python3
"""Clone and build OpenDS dependencies on the target.

Syncs the OpenDS tree to the target (for the step scripts), then runs
setup_xnvme.yaml which installs the modified xNVMe fork required by
the aisio backend. Run once to bootstrap; sync_and_build.py is enough
for iterating on OpenDS after that.
"""

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
    ["cijoe", "-m", "-s", "-o", "cijoe-output-setup-deps", *configs,
     "tasks/setup_xnvme.yaml"],
    cwd=root,
).returncode)
