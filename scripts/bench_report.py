#!/usr/bin/env python3
"""Render the README perf block from bench artifacts.

Reads artifacts written by the bench suites
(`cijoe-output-bench-<backend>/artifacts/`):

  meta.json                              -- commit, hostname, kernel, NVMe, GPU
  <backend>_<data_dir>.log               -- sync filperf stdout (CSV + summary)
  <backend>_<data_dir>_async.log         -- async filperf stdout (CSV + summary)

and rewrites the block between <!-- bench:start --> and
<!-- bench:end --> in README.md. Run scripts/run_bench.py first.
"""

import json
import re
import sys
from pathlib import Path

from _helpers import ok

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"
MARK_START = "<!-- bench:start -->"
MARK_END = "<!-- bench:end -->"

BACKENDS = ["gds", "opends"]
DATASETS = ["filesize8gib", "tiktokish", "imagenetish", "lmcacheish"]
MODES = ["sync", "async"]


def _artifacts_dir(backend):
    return ROOT / f"cijoe-output-bench-{backend}" / "artifacts"


def _parse_mib_per_s(log_path):
    """Return MiB/s from the filperf --summary block, or None."""
    if not log_path.is_file():
        return None
    m = re.search(r"^\s*MiB/s:\s*([0-9.]+)\s*$",
                  log_path.read_text(), re.MULTILINE)
    return float(m.group(1)) if m else None


def _log_path(backend, ds, mode):
    suffix = "_async" if mode == "async" else ""
    return _artifacts_dir(backend) / f"{backend}_{ds}{suffix}.log"


def _collect(backend):
    runs = {(ds, mode): _parse_mib_per_s(_log_path(backend, ds, mode))
            for ds in DATASETS for mode in MODES}
    meta_path = _artifacts_dir(backend) / "meta.json"
    meta = json.loads(meta_path.read_text()) if meta_path.is_file() else None
    return meta, runs


def _render(metas, results):
    meta = metas.get("opends") or metas.get("gds") or {}

    stamp = (f"_Commit `{meta.get('commit', '?')}` on host "
             f"`{meta.get('hostname', '?')}` (kernel `{meta.get('kernel', '?')}`, "
             f"NVMe `{meta.get('nvme_model', '?')}`, "
             f"GPU `{meta.get('gpu_model', '?')}`)._")

    header = ("| Dataset       | mode  | "
              + " | ".join(f"{b} (MiB/s)" for b in BACKENDS) + " |")
    sep    = ("|---------------|-------|"
              + "|".join(["-" * 14 for _ in BACKENDS]) + "|")
    rows = [header, sep]
    for ds in DATASETS:
        for mode in MODES:
            cells = []
            for b in BACKENDS:
                v = results[b].get((ds, mode))
                cells.append(f"{v:.0f}" if v is not None else "-")
            rows.append(f"| {ds:<13} | {mode:<5} | "
                        + " | ".join(f"{c:>12}" for c in cells) + " |")

    return stamp + "\n\n" + "\n".join(rows)


def main():
    metas = {}
    results = {}
    for b in BACKENDS:
        metas[b], results[b] = _collect(b)

    has_meta = any(metas.values())
    has_run = any(v for r in results.values() for v in r.values())
    if not has_meta and not has_run:
        print("no bench artifacts found; README unchanged", file=sys.stderr)
        return

    block = _render(metas, results)

    text = README.read_text()
    pattern = re.compile(
        re.escape(MARK_START) + r".*?" + re.escape(MARK_END),
        re.DOTALL,
    )
    if not pattern.search(text):
        sys.exit(f"README is missing {MARK_START}/{MARK_END} markers")
    README.write_text(pattern.sub(f"{MARK_START}\n{block}\n{MARK_END}", text))
    ok("bench_report")


if __name__ == "__main__":
    main()
