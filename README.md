# OpenDS

Open source accelerator direct storage. Vendor-neutral, drop-in
replacement for NVIDIA's cuFile (GDS), powered by aisio for
high-throughput PCIe P2P DMA from NVMe straight into GPU memory.

## Backends

- **Reference** (`libopends_ref`): POSIX `pread`/`pwrite` on host
  buffers. No external dependencies. Serves as a correctness baseline
  and template for hardware-specific backends.
- **GDS** (`libopends_gds`): Wraps NVIDIA cuFile for GPUDirect
  Storage. Buffers are GPU memory allocated with `cudaMalloc` and
  registered via `cuFileBufRegister`. Requires CUDA toolkit and the
  cuFile (GDS) library. Built conditionally when both are found.
- **aisio** (`libopends_aisio`): PCIe P2P DMA from NVMe into GPU
  memory via [xNVMe](https://xnvme.io)'s `upcie-cuda` backend (no
  filesystem or kernel `nvme` driver in the path). Based on
  [aisio](https://github.com/xnvme/aisio). File-to-LBA mapping uses
  a mock HOMI client backed by `fs_mock`; callers register extents
  (obtained from the live FS via XAL or FIEMAP) before the driver is
  unbound. Both mocks will be replaced by aisio's HOMI implementation.
  Requires xNVMe and the CUDA toolkit; read-only.

## Performance

Headline read throughput across the three reference datasets,
cold-cache, N=1. `scripts/bench_report.py` regenerates the block
below from bench artifacts; see "Benchmarking with filperf" for how
to run the suites.

<!-- bench:start -->
_Commit `a781ba2-dirty` on host `swissknife` (kernel `6.8.12-dmabuf`, NVMe `Samsung S4LV008[Pascal]`, GPU `NVIDIA RTX 2000 Ada Generation`)._

| Dataset       | mode  | gds (MiB/s) | opends (MiB/s) |
|---------------|-------|--------------|--------------|
| filesize8gib  | sync  |         6441 |         7124 |
| filesize8gib  | async |         2424 |         7118 |
| tiktokish     | sync  |         2725 |         4948 |
| tiktokish     | async |         2325 |         5078 |
| imagenetish   | sync  |          589 |          612 |
| imagenetish   | async |          835 |         3036 |
<!-- bench:end -->

## opends API

### Basic read

Reads must be LBA-aligned (both offset and size). The file must be
opened with `O_DIRECT`. Unaligned reads via bounce buffers are planned
but not yet implemented.

```c
#include <opends.h>
#include <cuda_runtime.h>
#include <fcntl.h>
#include <stdio.h>

int main(void)
{
    opends_driver_open();

    int fd = open("/mnt/nvme/data.bin", O_RDONLY | O_DIRECT);

    opends_handle_t fh;
    opends_handle_register(&fh, fd);

    size_t size = 1024 * 1024;
    void *buf;
    cudaMalloc(&buf, size);
    opends_buf_register(buf, size, 0);

    ssize_t nread = opends_read(fh, buf, size, 0, 0);
    printf("read %zd bytes\n", nread);

    opends_buf_deregister(buf);
    cudaFree(buf);
    opends_handle_deregister(fh);
    close(fd);
    opends_driver_close();

    return 0;
}
```

### Buffer offset

The last parameter to `opends_read` is a byte offset into the
destination buffer. This exists because GPU device pointers cannot be
dereferenced or offset from host code. Instead of pointer arithmetic,
pass the base pointer and let the backend apply the offset.

```c
/* Read two 4 KiB blocks into different regions of a device buffer. */
opends_read(fh, dev_buf, 4096, 0,    0);     /* -> dev_buf[0..4095]    */
opends_read(fh, dev_buf, 4096, 4096, 4096);  /* -> dev_buf[4096..8191] */
```

### Error handling

Functions returning `opends_error_t` carry both an opends error code
and an optional backend-specific code. Functions returning `ssize_t`
(read/write) return the byte count on success or a negated error on
failure.

```c
opends_error_t err = opends_handle_register(&fh, fd);

if (err.err != OPENDS_SUCCESS) {
    fprintf(stderr, "%s\n", opends_op_status_error(err.err));
}

ssize_t n = opends_read(fh, buf, size, offset, 0);
if (n < 0) {
    fprintf(stderr, "read: %s\n",
            opends_op_status_error((opends_op_error_t)-n));
}
```

## Building

Requires [Meson](https://mesonbuild.com) and a C11 compiler. The GDS
backend additionally requires the CUDA toolkit and cuFile library.

```sh
meson setup build
meson compile -C build
```

Meson reports which backends are enabled at configure time:

```
Backends
  Reference backend: true
  GDS backend      : true
  aisio backend    : true
```

## Installing

Install headers, libraries, and a pkg-config file so other projects can
find OpenDS via `pkg-config --cflags --libs opends` or meson's
`dependency('opends')`:

```sh
meson install -C build
```

## Testing

Run the reference backend smoke test locally:

```sh
./build/test_smoke_ref
```

Run the full synchronous-read suite against the ref backend locally.
`test_sync_read_prep` writes a deterministic 16-page pattern to a
file; each backend test reads it back through its backend and
verifies against an in-memory oracle:

```sh
f=$(mktemp) && ./build/test_sync_read_prep "$f" \
  && ./build/test_sync_read_ref "$f"; rm -f "$f"
```

### Remote testing with CIJOE

Integration tests run on a remote target via
[CIJOE](https://github.com/refenv/cijoe). Target requirements:

- A dedicated NVMe device (not the boot disk; the aisio phase
  unbinds it from the kernel `nvme` driver).
- An NVIDIA GPU with the CUDA toolkit; GDS (GPUDirect Storage) for
  the gds tests; xNVMe's `upcie-cuda` backend for the aisio tests.
- A kernel built with UDMABUF-import support, IOMMU disabled, and
  2 MiB hugepages allocated (prerequisites for the GPU↔NVMe dma-buf
  P2P path that aisio uses).
- An XFS filesystem on the test namespace; the mount step does not
  format. Test artifacts live under `<mount_point>/opends_tests/`.

The [aisio](https://github.com/xnvme/aisio) project ships cijoe
tasks that take a fresh Ubuntu 24.04 install through every step
above (custom kernel, NVIDIA stack, hugepages, XFS format, reference
datasets). Follow its README first to bring up a target that meets
these requirements. OpenDS then pins its own xNVMe/xal/fil refs in
`configs/deps.toml` and installs them via `scripts/setup_deps.py`
for reproducible test runs.

`test_sync_read_prep` writes both a pattern file and an extents file
(`sync_read_extents.bin`) while the FS is mounted. The ref and gds
tests read the pattern back through the kernel FS. The aisio phase
runs last: the kernel driver is unbound, then the aisio tests load
the extents file and register entries into `fs_mock` to resolve
handles without touching the (now unmounted) filesystem.

1. Copy the example configs and fill in target details:

   ```sh
   cp configs/transport.toml.example configs/transport.toml
   cp configs/test.toml.example configs/test.toml
   ```

   `configs/deps.toml` is tracked and needs no editing.

2. Bootstrap (first run only):

   ```sh
   python scripts/rsync.py
   python scripts/setup_deps.py   # Installs xNVMe, xal, fil
   python scripts/build.py
   ```

   Iterative loop: `python scripts/rsync.py && python scripts/build.py`.

3. Run all test suites:

   ```sh
   python scripts/run_tests.py
   ```

### Benchmarking with filperf

Throughput benchmarks use `filperf` from
[fil](https://github.com/xnvme/fil) against three reference datasets
(`filesize8gib`, `tiktokish`, `imagenetish`). Two suites:
`tasks/bench_gds.yaml` (cuFile, kernel `nvme` bound) and
`tasks/bench_opends.yaml` (OpenDS aisio, kernel driver unbound).
Datasets are populated by aisio's `tasks/setup_dataset.yaml` under
`config.test.mount_point`.

Prerequisites: `scripts/setup_deps.py` and `scripts/build.py` have
run on the target, and aisio's `tasks/setup_dataset.yaml` has
populated the three datasets.

Run, then render:

```sh
python scripts/run_bench.py [--suite gds|opends]
python scripts/bench_report.py
```

Each suite writes artifacts to
`cijoe-output-bench-<backend>/artifacts/`: `meta.json` (commit plus
host/kernel/NVMe/GPU info) and `<backend>_<dataset>.log` (verbatim
`filperf` stdout). `bench_report.py` parses these to rewrite the
perf block above. Each `filperf` drops page caches first so numbers
are cold-cache.
