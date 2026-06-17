#!/usr/bin/env python3
"""Run the thread-scaling aisio-vs-GDS comparison on the target.

Reads the dataset and read pattern from the [thread_scaling] config while
sweeping loader-thread count, printing a RESULT line per (backend, thread-count).
With no args it runs both phases -- GDS (kernel nvme + cuFile P2P) then aisio
(HOMI/qublk + upcie P2P) -- in one device flow; --backend runs just one. The
--threads / --reactors / --read-offset / --read-bytes flags override the matching
[thread_scaling] config value for this run.

scripts/build.py must have built OpenDS with both backends, and the dataset must
exist (scripts/setup_dataset.py).
"""

import argparse
import os
import tempfile
import tomllib

from _helpers import ROOT, ok, run_cijoe

# Steps per backend phase, in workflow order. cpu_governor pins the governor for
# either; gds needs nvidia_fs over the kernel mount; aisio mounts, hands the
# controller to HOMI, then tears it back down.
STEPS = {
    "gds":   ["cpu_governor", "load_nvidia_fs", "bind_nvme", "mount",
              "thread_scaling_gds"],
    "aisio": ["cpu_governor", "bind_nvme", "mount", "homi_stack_up",
              "thread_scaling_aisio", "homi_stack_down"],
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--backend", action="append", choices=list(STEPS),
    help="Run only this backend phase. Repeat to combine. Default: both.",
)
parser.add_argument("--threads", help='override [thread_scaling].threads, e.g. "1 2 4"')
parser.add_argument("--reactors", help='override [thread_scaling].reactors, e.g. "1 2 3"')
parser.add_argument("--read-offset", type=int, help="override [thread_scaling].read_offset")
parser.add_argument("--read-bytes", type=int, help="override [thread_scaling].read_bytes")
args = parser.parse_args()

backends = args.backend or list(STEPS)
if set(backends) == set(STEPS):
    steps = []  # both phases: run the full two-phase workflow as written
else:
    seen, steps = set(), []
    for b in ("gds", "aisio"):
        if b in backends:
            for s in STEPS[b]:
                if s not in seen:
                    seen.add(s)
                    steps.append(s)

overrides = {k: v for k, v in {
    "threads": args.threads,
    "reactors": args.reactors,
    "read_offset": args.read_offset,
    "read_bytes": args.read_bytes,
}.items() if v is not None}

extra_configs, overlay = [], None
if overrides:
    # Merge onto the base section so a partial override still produces a complete
    # [thread_scaling], regardless of cijoe's merge granularity. cijoe reads this
    # overlay last (see run_cijoe), so its values win.
    with open(ROOT / "configs" / "test.toml", "rb") as f:
        merged = {**tomllib.load(f).get("thread_scaling", {}), **overrides}
    fd, overlay = tempfile.mkstemp(suffix=".toml", prefix="thread_scaling_override_")
    with os.fdopen(fd, "w") as f:
        f.write("[thread_scaling]\n")
        for k, v in merged.items():
            f.write(f'{k} = "{v}"\n' if isinstance(v, str) else f"{k} = {v}\n")
    extra_configs = ["-c", overlay]

try:
    run_cijoe("tasks/bench_thread_scaling.yaml", *steps,
              out="cijoe-output-thread-scaling", extra_configs=extra_configs)
finally:
    if overlay:
        os.unlink(overlay)
ok(f"run_thread_scaling {','.join(backends)}")
