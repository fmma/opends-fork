# cuFile buffer registration vs no-registration, across datasets

Date: 2026-07-07. Host: swissknife.

Effect of `cuFileBufRegister` on GDS/cuFile read throughput, measured across the
four fil-format datasets on the 990 PRO. Registration is the default; "no-register"
skips it (the path the PR's `--no-buf-register` flag exposes).

## Headline

The benefit of registration is a function of file size, and it **crosses over**:
registration hurts for tiny files, helps most at medium sizes, and converges as
throughput approaches the device ceiling.

| Dataset | Avg file size | Config (bs x batches) | Register (MiB/s) | No-register (MiB/s) | Register vs no-register |
|---|---:|---|---:|---:|---:|
| `imagenetish`  | ~110 KB | 128 x 80 | 354  | 640  | **-45%** (register hurts) |
| `tiktokish`    | ~7.6 MB | 64 x 20  | 4432 | 2975 | **+49%** |
| `lmcacheish`   | ~14 MB  | 32 x 10  | 5306 | 4400 | **+21%** |
| `filesize8gib` | ~8 GiB  | 1 x 3    | 6990 | 6467 | **+8%** |

Values are the mean of the per-rep runs in `sweep.csv` (imagenetish: 5 reps incl. a
clean uncontended re-measure; others: 2 reps). Reps were tight (<1% spread).

## Interpretation

- **Tiny files (110 KB): registration is a net loss (-45%).** The per-batch
  registration/DMA-setup cost is not amortized over such small transfers; the
  unregistered path wins by ~1.8x.
- **Medium files (7.6 MB): biggest win (+49%).** Registration unlocks the P2P
  fast path where it pays off most.
- **Large to huge: diminishing win (+21% -> +8%).** Absolute throughput climbs
  toward the ~7 GB/s device ceiling, where registered and unregistered both
  saturate and converge.

Takeaway: `cuFileBufRegister` is clearly right for medium/large files but
counterproductive for many-tiny-file workloads, which argues for the PR's
`--no-buf-register` escape hatch (or making registration size-gated).

## Environment

- GPU: NVIDIA RTX 2000 Ada. CUDA 12.8, cuFile (GDS).
- `nvidia_fs` kernel module loaded, so this is the real GPUDirect Storage P2P
  path (not cuFile compat mode) where buffer registration is relevant.
- Storage: Samsung 990 PRO NVMe at `0000:01:00.0`, whole-device XFS, kernel
  `nvme` driver, mounted at `/mnt/nvme`.
- Tool: fil `filperf`, backend `gds`.
- Command: `filperf /dev/nvme0n1 --backend gds --mnt /mnt/nvme --data-dir <DS> --batch-size <BS> --batches <NB> --summary`
- Metric: IO-time throughput (MiB/s) from `--summary`, which excludes the
  per-invocation XAL device-index/prep time.

## Methodology and caveat (proxy measurement)

These are **fil poc-branch numbers used as a faithful proxy** for PR
[`fmma/fil#10`](https://github.com/xnvme/fil/pull/10) (`fix/cufile-buf-register`,
which adds the `--no-buf-register` flag). The PR branch does not build on
swissknife: its new-`main` aisio/GPU backends require an xnvme fork exposing
`xnvme_opts.device_heap_size`, which is not present on the box, and fil compiles
all backends together.

The poc branch (`94c4926`) builds on the installed toolchain (xnvme 0.7.5 + xal
0.2.0) and contains the same `cuFileBufRegister` call the PR toggles:

- **register** = poc tip; `libfil.so` imports `cuFileBufRegister`.
- **no-register** = poc with the register commit `80fb7ca` reverted; `libfil.so`
  omits it.

Verified the two `libfil.so` differ and each `filperf` loads its own via rpath.
The register call is identical in effect to the PR's, so the delta reflects the
PR flag off vs on; measuring the PR's actual binary would need the xnvme fork
built on the box first.

Raw per-rep data: [`sweep.csv`](sweep.csv).
