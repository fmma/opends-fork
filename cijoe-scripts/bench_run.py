"""Run a single filperf bench and stash its stdout as an artifact.

The artifact is the verbatim filperf output (CSV plus `--summary`
block) written to
artifacts/<backend>_<data_dir>[_async].log (the `_async` suffix is
only appended when `mode: async`). Parsing is deferred to
scripts/bench_report.py so the cijoe step stays oblivious to
filperf's exact output shape.

Each bench run drops the page cache and runs filperf under
`prlimit --nofile`. nofile is required for opends (one dma-buf fd
per 2 MiB chunk on NVIDIA) and harmless elsewhere; drop_caches is
the cold-cache baseline for gds and a no-op for opends (the device
is unbound from the kernel nvme driver).
"""

import logging as log
from pathlib import Path

NOFILE = 1048576


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
    if args.backend == "gds":
        repo = cijoe.getconf("test.repo_path")
        err, state = cijoe.run(f"'{repo}/tasks/steps/resolve_nvme_ns.sh' '{bdf}'")
        if err:
            return err
        target = state.output().strip().splitlines()[-1]
    else:
        target = bdf

    filperf = [
        f"prlimit --nofile={NOFILE}:{NOFILE} --",
        f"filperf '{target}'",
        f"--backend {args.backend}",
        f"--data-dir {args.data_dir}",
        f"--batches {args.batches} --batch-size {args.batch_size}",
        "--summary",
    ]
    if args.mnt:
        filperf.insert(2, f"--mnt '{args.mnt}'")
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
    return 0
