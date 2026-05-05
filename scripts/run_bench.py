#!/usr/bin/env python3
"""Run the filperf-driven bench suite on the target.

Requires aisio's configs/datasets.toml (which describes the on-target
XFS dataset layout) to be passed via --datasets-config. aisio's
tasks/setup_dataset.yaml must have been run on the target first so the
dataset paths are populated.
"""

import argparse
import subprocess
import sys
from pathlib import Path

from _helpers import fail

root = Path(__file__).resolve().parent.parent

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--datasets-config",
    required=True,
    help="Path to aisio's configs/datasets.toml describing the dataset layout",
)
args = parser.parse_args()

datasets_config = Path(args.datasets_config).resolve()
if not datasets_config.is_file():
    sys.exit(f"datasets config not found: {datasets_config}")

configs = ["-c", "configs/transport.toml", "-c", "configs/deps.toml",
           "-c", "configs/test.toml", "-c", str(datasets_config)]

rc = subprocess.run(
    ["cijoe", "-m", "-s", "-o", "cijoe-output-bench", *configs,
     "tasks/bench_opends.yaml"],
    cwd=root,
).returncode
if rc:
    fail(f"run_bench (rc={rc})")
sys.exit(rc)
