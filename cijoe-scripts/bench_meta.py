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
from datetime import datetime, timezone
from pathlib import Path


def _target_stdout(cijoe, cmd):
    err, state = cijoe.run(cmd)
    if err:
        log.error(f"failed({cmd}); err({err})")
        return None
    return state.output().strip()


def main(args, cijoe):
    repo_path = cijoe.getconf("test.repo_path")
    nvme_bdf = cijoe.getconf("test.nvme_bdf", "")

    nvme_model = ""
    if nvme_bdf:
        line = _target_stdout(cijoe, f"lspci -s '{nvme_bdf}'") or ""
        nvme_model = line.split(": ", 1)[-1]

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
