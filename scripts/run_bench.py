#!/usr/bin/env python3
"""Run filperf-driven bench suites on the target.

aisio's tasks/setup_dataset.yaml must have been run on the target
first so the three reference datasets (filesize8gib, tiktokish,
imagenetish) exist under config.test.mount_point.

Each suite runs every dataset in both sync and async mode by default.
Use --mode to restrict to sync or async, and --dataset to restrict
to one or more datasets (repeat to combine). When restricting, the
relevant setup and teardown steps still run; bench_<dataset> /
bench_<dataset>_async steps are filtered.
"""

import argparse

from _helpers import ok, run_cijoe

SUITES = {
    "gds": "tasks/bench_gds.yaml",
    "opends": "tasks/bench_opends.yaml",
}

DATASETS = ["filesize8gib", "tiktokish", "imagenetish", "lmcacheish"]

SETUP_STEPS = {
    "gds":    ["cpu_governor", "load_nvidia_fs", "meta", "bind_nvme", "mount",
               "gen_lmcache"],
    "opends": ["cpu_governor", "meta", "homi_stack_up", "gen_lmcache"],
}
TEARDOWN_STEPS = {
    "gds":    [],
    "opends": ["homi_stack_down"],
}


def _steps_for(backend, mode, datasets):
    """Step name list to pass to cijoe; empty list means run all steps."""
    if mode == "both" and set(datasets) == set(DATASETS):
        return []
    modes = ["sync", "async"] if mode == "both" else [mode]
    bench = [
        f"{backend}_{ds}" + ("_async" if m == "async" else "")
        for ds in datasets for m in modes
    ]
    return SETUP_STEPS[backend] + bench + TEARDOWN_STEPS[backend]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--suite", action="append", choices=list(SUITES),
    help="Run only this suite. Repeat to combine. Default: all.",
)
parser.add_argument(
    "--mode", choices=["sync", "async", "both"], default="both",
    help="Restrict to sync or async bench steps. Default: both.",
)
parser.add_argument(
    "--dataset", action="append", choices=DATASETS,
    help="Run only this dataset. Repeat to combine. Default: all.",
)
args = parser.parse_args()

for name in args.suite or list(SUITES):
    steps = _steps_for(name, args.mode, args.dataset or DATASETS)
    print(f"\n=== {name} suite: {', '.join(steps) if steps else 'all steps'} ===",
          flush=True)
    run_cijoe(SUITES[name], *steps, out=f"cijoe-output-bench-{name}")
ok("run_bench")
