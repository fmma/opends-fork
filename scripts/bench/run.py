#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Sweep the filperf-driven bench suites on the target.

Every suite sweeps its own knob grid into cijoe-output-bench/<suite>/.
gds has no knobs: its sweep is the singleton, one run of the whole suite.
opends runs the point list in scripts/bench/sweep.toml, the parts of the grid
that carry information, about 40 minutes. --full-sweep runs the whole
io_threads x queue_depth x assume_aligned_only x idle_spin cross product
instead, 80 legs and about 6 hours on the default axes. Narrowing an axis
(--io-threads, --queue-depth, --assume-aligned-only, --idle-spin) sweeps the
cross product too, over the axes as given. --sweep-config swaps in another
list.

Either way the HOMI/qublk stack comes up once, each point runs the bench steps
into opends/t<t>_q<q>[_aligned][_spin<v>][_busy]/ with the aisio knobs passed
through the environment, and the stack is torn down. --busy-spin applies
OPENDS_AISIO_BUSY_SPIN=1 to the whole invocation. Both suites set the CPU
governor for the run and restore it in teardown. Restrict with --suite,
--mode, --dataset.

An assume_aligned_only leg rejects any read with a sub-LBA tail, so datasets
whose files are not LBA-multiples fail by construction. Those legs are recorded
with an empty result and the sweep carries on to the datasets that do qualify.

aisio's tasks/setup_dataset.yaml must have populated the reference datasets
(filesize8gib, tiktokish, imagenetish) under config.test.mount_point, and
scripts/bench/setup_dataset.py must have populated lmcacheish (both one-time).
"""

import argparse
import os
import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from _helpers import fail, ok, run_cijoe

SUITES = {
    "gds": "tasks/bench_gds.yaml",
    "opends": "tasks/bench_opends.yaml",
}

DATASETS = ["filesize8gib", "tiktokish", "imagenetish", "lmcacheish"]

DEFAULT_POINTS = Path(__file__).resolve().parent / "sweep.toml"

GRID_AXES = {"io_threads": [1, 2, 4, 8],
             "queue_depth": [1, 2, 4, 8, 16, 32, 64, 128, 256, 512],
             "assume_aligned_only": [0, 1],
             "idle_spin": [None]}

GDS_SETUP = ["cpu_governor", "load_nvidia_fs", "meta", "bind_nvme", "mount"]


def _bench_steps(backend, mode, datasets):
    modes = ["sync", "async"] if mode == "both" else [mode]
    return [f"{backend}_{ds}" + ("_async" if m == "async" else "")
            for ds in datasets for m in modes]


def _int_list(s):
    return [int(x) for x in s.split(",") if x.strip()]


def _spin_list(s):
    """Comma list of idle-spin values: "default" keeps the library default
    (the env var stays unset), an integer is microseconds (0 naps
    immediately, the pre-idle-spin behavior)."""
    out = []
    for x in s.split(","):
        x = x.strip()
        if not x:
            continue
        out.append(None if x == "default" else int(x))
    return out


def _run_gds(args, datasets):
    """Singleton sweep: the gds suite has no knobs to grid over."""
    if args.mode == "both" and set(datasets) == set(DATASETS):
        steps = []
    else:
        steps = GDS_SETUP + _bench_steps("gds", args.mode, datasets)
    print(f"\n=== gds suite: "
          f"{', '.join(steps) if steps else 'all steps'} ===", flush=True)
    try:
        run_cijoe(SUITES["gds"], *steps, out=f"{args.out}/gds")
    finally:
        run_cijoe(SUITES["gds"], "cpu_governor_restore",
                  out=f"{args.out}/gds/_teardown")


def _grid_points(args):
    """The full cross product, in sweep order."""
    axis = {k: getattr(args, k) or v for k, v in GRID_AXES.items()}
    for s in axis["idle_spin"]:
        for a in axis["assume_aligned_only"]:
            for t in axis["io_threads"]:
                for q in axis["queue_depth"]:
                    yield {"io_threads": t, "queue_depth": q,
                           "assume_aligned_only": a, "idle_spin_us": s}


POINT_KEYS = {"io_threads", "queue_depth", "datasets", "mode",
              "assume_aligned_only", "idle_spin_us", "name", "why"}


def _file_points(path, fail):
    """Read a point list, e.g. scripts/bench/sweep.toml."""
    try:
        with open(path, "rb") as f:
            doc = tomllib.load(f)
    except (OSError, tomllib.TOMLDecodeError) as exc:
        fail(f"{path}: {exc}")
    points = doc.get("point")
    if not points:
        fail(f"{path}: no [[point]] entries")
    for i, p in enumerate(points, 1):
        where = f"{path}: point {i}"
        unknown = set(p) - POINT_KEYS
        if unknown:
            fail(f"{where} has unknown keys {', '.join(sorted(unknown))}")
        for key in ("io_threads", "queue_depth"):
            if not isinstance(p.get(key), int):
                fail(f"{where} needs an integer {key}")
        unknown = set(p.get("datasets") or ()) - set(DATASETS)
        if unknown:
            fail(f"{where} names unknown datasets "
                 f"{', '.join(sorted(unknown))}")
        if p.get("mode", "both") not in ("sync", "async", "both"):
            fail(f"{where} has an unknown mode {p['mode']!r}")
    return points


def _run_opends(args, datasets, points):
    """Run the points; returns the legs cijoe reported a failure for."""
    failed = []
    suite = SUITES["opends"]
    out = f"{args.out}/opends"
    if args.busy_spin:
        os.environ["OPENDS_AISIO_BUSY_SPIN"] = "1"
    else:
        os.environ.pop("OPENDS_AISIO_BUSY_SPIN", None)
    if not args.skip_setup:
        run_cijoe(suite, "cpu_governor", "homi_stack_up", out=f"{out}/_setup")
    try:
        for p in points:
            t, q = p["io_threads"], p["queue_depth"]
            a = int(p.get("assume_aligned_only", 0))
            s = p.get("idle_spin_us")
            if s is None:
                os.environ.pop("OPENDS_AISIO_IDLE_SPIN_US", None)
            else:
                os.environ["OPENDS_AISIO_IDLE_SPIN_US"] = str(s)
            os.environ["OPENDS_AISIO_IO_THREADS"] = str(t)
            os.environ["OPENDS_AISIO_QUEUE_DEPTH"] = str(q)
            os.environ["OPENDS_AISIO_ASSUME_ALIGNED_ONLY"] = str(a)
            leg = p.get("name") or (f"t{t}_q{q}" + ("_aligned" if a else "")
                                    + (f"_spin{s}" if s is not None else "")
                                    + ("_busy" if args.busy_spin else ""))
            steps = _bench_steps("opends", p.get("mode", args.mode),
                                 p.get("datasets") or datasets)
            print(f"\n=== opends io_threads={t} queue_depth={q} "
                  f"assume_aligned_only={a}"
                  + (f" idle_spin_us={s}" if s is not None else "")
                  + (" busy_spin=1" if args.busy_spin else "")
                  + f": {', '.join(steps)} ===", flush=True)
            rc = run_cijoe(suite, "meta", *steps, out=f"{out}/{leg}",
                           check=False)
            if rc:
                print(f"leg {leg} failed (rc={rc}); continuing", flush=True)
                failed.append(leg)
    finally:
        if not args.keep_stack:
            run_cijoe(suite, "homi_stack_down", "cpu_governor_restore",
                      out=f"{out}/_teardown")
    return failed


parser = argparse.ArgumentParser(
    description=__doc__,
    formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--suite", action="append", choices=list(SUITES),
                    help="Run only this suite. Repeat to combine. Default: "
                         "all.")
parser.add_argument("--mode", choices=["sync", "async", "both"], default="both")
parser.add_argument("--dataset", action="append", choices=DATASETS,
                    help="Run only this dataset. Repeat to combine.")
parser.add_argument("--batches", action="append", metavar="DS=N",
                    help="Override batches for a dataset (e.g. tiktokish=50) "
                         "to lengthen its measurement window. Repeatable. "
                         "Forwarded via OPENDS_BENCH_BATCHES; batch-size, the "
                         "concurrency under study, is unchanged.")
parser.add_argument("--cpu-mask", metavar="MASK",
                    help="Pin aisio IO workers round-robin to the CPUs in "
                         "this hex mask (e.g. 0x3f = cores 0-5); 0x0 or "
                         "unset leaves placement to the scheduler. Forwarded "
                         "via OPENDS_AISIO_CPU_MASK; opends suite only.")
parser.add_argument("--out", default="cijoe-output-bench",
                    help="Output dir root; each suite writes under "
                         "<out>/<suite>/. Default cijoe-output-bench. A full "
                         "sweep and a point list share their leg names, so "
                         "give one of the two a distinct dir to keep both "
                         "(cijoe wipes a reused leg dir).")

grid = parser.add_argument_group("opends sweep points")
which = grid.add_mutually_exclusive_group()
which.add_argument("--full-sweep", action="store_true",
                   help="Sweep the whole io_threads x queue_depth x "
                        "assume_aligned_only x idle_spin cross product "
                        "instead of the point list: 80 legs and about 6 "
                        "hours on the default axes, 160 legs and 13 hours "
                        "with --idle-spin default,0. Narrowing an axis below "
                        "implies this mode, so the flag is only needed to "
                        "sweep every axis in full.")
which.add_argument("--sweep-config", metavar="FILE",
                   help="Run the points listed in FILE (TOML) instead of the "
                        "default list, scripts/bench/sweep.toml. A point "
                        "names io_threads and queue_depth, and may narrow "
                        "datasets and mode, so a costly dataset runs only "
                        "where its own optimum is.")
grid.add_argument("--io-threads", type=_int_list,
                  help="Comma-separated IO-thread counts, which sweeps the "
                       "cross product over them. Default 1,2,4,8.")
grid.add_argument("--queue-depth", type=_int_list,
                  help="Comma-separated NVMe queue depths. Default 1..512 "
                       "(>512 may exceed the device MQES and fail).")
grid.add_argument("--assume-aligned-only", type=_int_list,
                  metavar="LIST",
                  help="Comma-separated OPENDS_AISIO_ASSUME_ALIGNED_ONLY "
                       "values. Default 0,1 (both), which doubles the grid; "
                       "pass 0 for the tail-fixup sweep alone. An aligned leg "
                       "writes to t<t>_q<q>_aligned/, so 0-legs keep their "
                       "paths.")
grid.add_argument("--idle-spin", type=_spin_list,
                  metavar="LIST",
                  help="Comma-separated OPENDS_AISIO_IDLE_SPIN_US values in "
                       "microseconds; the token 'default' keeps the library "
                       "default (env unset), 0 naps immediately (the "
                       "pre-idle-spin behavior). E.g. 'default,0' doubles the "
                       "grid into an on/off comparison. An explicit value "
                       "writes to t<t>_q<q>_spin<v>/, so default legs keep "
                       "their paths. Default: 'default'.")
grid.add_argument("--busy-spin", action="store_true",
                  help="Set OPENDS_AISIO_BUSY_SPIN=1 for every leg of this "
                       "invocation (workers poll flat out; pair with "
                       "--cpu-mask). Legs write to t<t>_q<q>[...]_busy/.")
grid.add_argument("--skip-setup", action="store_true",
                  help="Assume the HOMI stack is already up.")
grid.add_argument("--keep-stack", action="store_true",
                  help="Leave the HOMI stack up after the sweep.")
args = parser.parse_args()

datasets = args.dataset or DATASETS
if args.batches:
    for spec in args.batches:
        ds, _, n = spec.partition("=")
        if not ds or not n.isdigit() or int(n) == 0:
            parser.error("--batches expects <dataset>=<positive integer>, "
                         f"got {spec!r}")
    os.environ["OPENDS_BENCH_BATCHES"] = ",".join(args.batches)
if args.cpu_mask:
    os.environ["OPENDS_AISIO_CPU_MASK"] = args.cpu_mask

axes = [f"--{k.replace('_', '-')}" for k in GRID_AXES
        if getattr(args, k) is not None]
if axes and args.sweep_config:
    parser.error(f"{', '.join(axes)} narrows a cross product, which "
                 f"--sweep-config {args.sweep_config} replaces")

points = (list(_grid_points(args)) if args.full_sweep or axes
          else _file_points(args.sweep_config or DEFAULT_POINTS,
                            parser.error))

failed = []
for name in args.suite or list(SUITES):
    if name == "gds":
        _run_gds(args, datasets)
    else:
        failed += _run_opends(args, datasets, points)

if failed:
    fail(f"run_bench: {len(failed)} of {len(points)} legs failed: "
         f"{', '.join(failed)}")
    sys.exit(1)
ok("run_bench")
