# SPDX-License-Identifier: BSD-3-Clause
"""Run a single filperf bench and stash its stdout as an artifact.

The artifact is the verbatim filperf output (CSV plus `--summary`
block) written to
artifacts/<backend>_<data_dir>[_async].log (the `_async` suffix is
only appended when `mode: async`). Parsing is deferred to
scripts/bench_report.py so the cijoe step stays oblivious to
filperf's exact output shape.

Each bench run runs filperf under `prlimit --nofile`. nofile is
required for opends (one dma-buf fd per 2 MiB chunk on NVIDIA) and
harmless elsewhere. gds runs get a cold-cache `drop_caches`; opends
reads files on the HOMI/qublk mount via DMA into GPU memory, so the
kernel page cache is irrelevant and the device's BDF/socket are
passed through OPENDS_HOMI_DEV/OPENDS_HOMI_SOCKET.
"""

import json
import logging as log
import re
from pathlib import Path

NOFILE = 1048576

SUMMARY_FIELDS = {
    "Total time": "total_time_s",
    "Prep time": "prep_time_s",
    "IO time": "io_time_s",
    "File/s": "files_per_s",
    "MiB/s": "mib_s",
    "IOPS": "iops",
    "Number of IOs": "num_ios",
}


INT_FIELDS = {"num_ios"}


def _parse_summary(text):
    out = {}
    for label, field in SUMMARY_FIELDS.items():
        m = re.search(rf"^\s*{re.escape(label)}:\s*([0-9.]+)\s*$", text, re.MULTILINE)
        if m:
            out[field] = int(float(m.group(1))) if field in INT_FIELDS \
                else float(m.group(1))
    return out


def _append_record(cijoe, args, log_name, text):
    artifacts = Path(cijoe.output_path) / "artifacts"
    meta_path = artifacts / "meta.json"
    if not meta_path.is_file():
        log.warning("no meta.json; skipping history record")
        return
    meta = json.loads(meta_path.read_text())
    env_keys = ("hostname", "kernel", "cuda", "nvme_bdf", "nvme_model",
                "gpu_model", "hugepages_2m")
    record = {
        "schema_version": 1,
        "run_id": f"{meta['timestamp']}-{meta['hostname']}",
        "run_timestamp": meta["timestamp"],
        "opends": {"sha": meta.get("commit_sha", ""),
                   "dirty": meta.get("dirty", False)},
        "deps": meta.get("deps", {}),
        "env": {k: meta.get(k) for k in env_keys},
        "config": {
            "backend": args.backend,
            "dataset": args.data_dir,
            "mode": args.mode,
            "cold_cache": True,
            "batches": args.batches,
            "batch_size": args.batch_size,
            "n": 1,
        },
        "result": _parse_summary(text),
        "raw_log": log_name,
    }
    with (artifacts / "history.jsonl").open("a") as f:
        f.write(json.dumps(record) + "\n")


def add_args(parser):
    parser.add_argument("--backend", required=True)
    parser.add_argument("--data-dir", required=True)
    parser.add_argument("--batches", type=int, required=True)
    parser.add_argument("--batch-size", type=int, required=True)
    parser.add_argument("--mnt", default="")
    parser.add_argument("--mode", choices=["sync", "async"], default="sync")


def main(args, cijoe):
    print(f"--- {args.backend} {args.data_dir} {args.mode} "
          f"(batches={args.batches} batch_size={args.batch_size}) ---",
          flush=True)
    bdf = cijoe.getconf("test.nvme_bdf")
    env = ""
    mnt = args.mnt
    if args.backend == "gds":
        repo = cijoe.getconf("test.repo_path")
        err, state = cijoe.run(f"'{repo}/tasks/steps/resolve_nvme_ns.sh' '{bdf}'")
        if err:
            return err
        target = state.output().strip().splitlines()[-1]
    elif args.backend == "opends":
        # HOMI owns the controller; qublk re-exports it as a ublk block device
        # that fil walks with xal for enumeration, exactly like gds walks the
        # kernel-bound NVMe. The aisio backend attaches a qpair from HOMI per
        # file (via OPENDS_HOMI_DEV/SOCKET), so the device read path bypasses
        # the ublk target.
        err, state = cijoe.run("cat /run/homi/ublk_dev")
        if err:
            return err
        target = state.output().strip().splitlines()[-1]
        sock = "/run/homi/homi.sock"
        if not mnt:
            mnt = cijoe.getconf("test.mount_point")
        env = (
            f"OPENDS_HOMI_DEV='{bdf}' OPENDS_HOMI_SOCKET='{sock}' "
            f"OPENDS_HOMI_MNT='{mnt}' "
        )
    else:
        target = bdf

    filperf = [
        f"{env}prlimit --nofile={NOFILE}:{NOFILE} --",
        f"filperf '{target}'",
        f"--backend {args.backend}",
        f"--data-dir {args.data_dir}",
        f"--batches {args.batches} --batch-size {args.batch_size}",
        "--summary",
    ]
    if mnt:
        filperf.insert(2, f"--mnt '{mnt}'")
    if args.mode == "async":
        filperf.append("--async")

    cmd = "echo 3 > /proc/sys/vm/drop_caches\n" + " \\\n  ".join(filperf)
    err, state = cijoe.run(cmd)
    if err:
        return err

    suffix = "_async" if args.mode == "async" else ""
    out = (Path(cijoe.output_path) / "artifacts"
           / f"{args.backend}_{args.data_dir}{suffix}.log")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(state.output())
    log.info(f"wrote {out}")
    _append_record(cijoe, args, out.name, state.output())
    return 0
