# SPDX-License-Identifier: BSD-3-Clause
"""Capture build commit and target platform info to artifacts/meta.json.

Runs early in a bench suite so its results carry the commit hash and
host/kernel/NVMe/GPU they were taken on. The OpenDS commit comes from
`<repo_path>/.commit_stamp` on the target (written by
scripts/rsync.py); reading from the target instead of the host avoids
the decoupling where host edits after rsync would mislabel binaries
built from the prior tree.

Dependency SHAs (xnvme, xal, homi, qublk, fil) are resolved from their
checkouts under config.deps.src_root, so a throughput change is
attributable to OpenDS versus a dep rebase under the same branch pin.
"""

import json
import logging as log
import os
import re
import shlex
from datetime import datetime, timezone
from pathlib import Path

DEPS = ["xnvme", "xal", "homi", "qublk", "fil"]


def _target_stdout(cijoe, cmd):
    err, state = cijoe.run(cmd)
    if err:
        log.error(f"failed({cmd}); err({err})")
        return None
    return state.output().strip()


def _short_nvme(machine_readable):
    """Render a tighter NVMe identifier from `lspci -mm -s <bdf>` output.

    The verbose `lspci -s` form prefixes the device with the full legal
    vendor name ("Samsung Electronics Co Ltd") and a generic `NVMe SSD
    Controller` qualifier, making the README platform stamp unwieldy.
    Take the short vendor (first token) plus the device string with
    that qualifier stripped, e.g. `Samsung S4LV008[Pascal]`.
    """
    parts = shlex.split(machine_readable)
    if len(parts) < 4:
        return machine_readable
    vendor = parts[2].split()[0]
    device = re.sub(r"^(?:NVMe SSD Controller|NVMe Controller)\s+", "", parts[3])
    return f"{vendor} {device}".strip()


def _split_commit(stamp):
    """Split a `<sha>` or `<sha>-dirty` stamp into (sha, dirty)."""
    if not stamp:
        return "", False
    dirty = stamp.endswith("-dirty")
    return (stamp[: -len("-dirty")] if dirty else stamp), dirty


def _cuda_version(cijoe):
    out = _target_stdout(
        cijoe, "/usr/local/cuda/bin/nvcc --version 2>/dev/null || nvcc --version",
    ) or ""
    m = re.search(r"release ([0-9]+\.[0-9]+)", out)
    return m.group(1) if m else ""


def _hugepages_2m(cijoe):
    out = _target_stdout(
        cijoe, "cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages",
    )
    return int(out) if out and out.isdigit() else None


def _dep_versions(cijoe, src_root):
    deps = {}
    for dep in DEPS:
        d = f"{src_root}/{dep}"
        sha = _target_stdout(cijoe, f"git -C '{d}' rev-parse HEAD 2>/dev/null") or ""
        status = _target_stdout(
            cijoe, f"git -C '{d}' status --porcelain 2>/dev/null",
        )
        deps[dep] = {
            "sha": sha,
            "ref": cijoe.getconf(f"{dep}.repository.ref", ""),
            "dirty": bool(status),
        }
    return deps


def main(args, cijoe):
    repo_path = cijoe.getconf("test.repo_path")
    src_root = cijoe.getconf("deps.src_root", "")
    nvme_bdf = cijoe.getconf("test.nvme_bdf", "")

    nvme_model = ""
    if nvme_bdf:
        mm = _target_stdout(cijoe, f"lspci -mm -s '{nvme_bdf}'") or ""
        nvme_model = _short_nvme(mm) if mm else ""

    stamp = _target_stdout(cijoe, f"cat '{repo_path}/.commit_stamp'")
    commit_sha, dirty = _split_commit(stamp)

    meta = {
        "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "commit": stamp,
        "commit_sha": commit_sha,
        "dirty": dirty,
        "hostname": _target_stdout(cijoe, "uname -n"),
        "kernel": _target_stdout(cijoe, "uname -r"),
        "cuda": _cuda_version(cijoe),
        "hugepages_2m": _hugepages_2m(cijoe),
        "nvme_bdf": nvme_bdf,
        "nvme_model": nvme_model,
        "gpu_model": _target_stdout(
            cijoe, "nvidia-smi --query-gpu=name --format=csv,noheader | head -1",
        ) or "",
        "deps": _dep_versions(cijoe, src_root) if src_root else {},
    }

    out = Path(cijoe.output_path) / "artifacts" / "meta.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(meta, indent=2) + "\n")
    log.info(f"wrote {out}")

    if os.environ.get("OPENDS_BENCH_STRICT") and (
        meta["dirty"] or any(d.get("dirty") for d in meta["deps"].values())
    ):
        log.error("OPENDS_BENCH_STRICT: refusing to bench a dirty tree "
                  "(OpenDS or a dependency)")
        return 1
    return 0
