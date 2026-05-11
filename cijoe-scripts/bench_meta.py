"""Capture build commit and target platform info to artifacts/meta.json.

Runs early in a bench suite so its results carry the commit hash and
host/kernel/NVMe/GPU they were taken on. The commit comes from
`<repo_path>/.commit_stamp` on the target (written by
scripts/rsync.py); reading from the target instead of the host avoids
the decoupling where host edits after rsync would mislabel binaries
built from the prior tree.
"""

import json
import logging as log
import re
import shlex
from datetime import datetime, timezone
from pathlib import Path


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


def main(args, cijoe):
    repo_path = cijoe.getconf("test.repo_path")
    nvme_bdf = cijoe.getconf("test.nvme_bdf", "")

    nvme_model = ""
    if nvme_bdf:
        mm = _target_stdout(cijoe, f"lspci -mm -s '{nvme_bdf}'") or ""
        nvme_model = _short_nvme(mm) if mm else ""

    meta = {
        "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "commit": _target_stdout(cijoe, f"cat '{repo_path}/.commit_stamp'"),
        "hostname": _target_stdout(cijoe, "uname -n"),
        "kernel": _target_stdout(cijoe, "uname -r"),
        "nvme_bdf": nvme_bdf,
        "nvme_model": nvme_model,
        "gpu_model": _target_stdout(
            cijoe, "nvidia-smi --query-gpu=name --format=csv,noheader | head -1",
        ) or "",
    }

    out = Path(cijoe.output_path) / "artifacts" / "meta.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(meta, indent=2) + "\n")
    log.info(f"wrote {out}")
    return 0
