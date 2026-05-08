#!/usr/bin/env python3
"""Run filperf-driven bench suites on the target.

aisio's tasks/setup_dataset.yaml must have been run on the target
first so the three reference datasets (filesize8gib, tiktokish,
imagenetish) exist under config.test.mount_point.
"""

import argparse

from _helpers import ok, run_cijoe

SUITES = {
    "gds": "tasks/bench_gds.yaml",
    "opends": "tasks/bench_opends.yaml",
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--suite", action="append", choices=list(SUITES),
    help="Run only this suite. Repeat to combine. Default: all.",
)
args = parser.parse_args()

for name in args.suite or list(SUITES):
    run_cijoe(SUITES[name], out=f"cijoe-output-bench-{name}")
ok("run_bench")
