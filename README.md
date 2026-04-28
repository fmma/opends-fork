# OpenDS

Open source accelerator direct storage. Provides a vendor-neutral
ds_file API for reading files directly into accelerator memory.

## Backends

- **Reference** (`libopends_ref`): POSIX `pread`/`pwrite` on host
  buffers. No external dependencies. Serves as a correctness baseline
  and template for hardware-specific backends.
- **GDS** (`libopends_gds`): Wraps NVIDIA cuFile for GPUDirect
  Storage. Buffers are GPU memory allocated with `cudaMalloc` and
  registered via `cuFileBufRegister`. Requires CUDA toolkit and the
  cuFile (GDS) library. Built conditionally when both are found.
- **aisio** (`libopends_aisio`): Reads directly from an NVMe device
  into GPU memory via [xNVMe](https://xnvme.io)'s `upcie-cuda` backend
  (PCIe P2P DMA, no filesystem or kernel nvme driver in the path).
  Based on [aisio](https://github.com/xnvme/aisio); file-to-LBA
  mapping comes from a mock HOMI client that loads an extent cache
  produced by `cache_extents` while the filesystem is still mounted.
  The mock will be replaced by aisio's HOMI implementation. Requires
  xNVMe and the CUDA toolkit. Read-only; `ds_file_write` returns
  `DS_FILE_IO_NOT_SUPPORTED`.

## ds_file API

### Basic read

Reads must be LBA-aligned (both offset and size). The file must be
opened with `O_DIRECT`. Unaligned reads via bounce buffers are planned
but not yet implemented.

```c
#include <opends.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    ds_file_error_t err = ds_file_driver_open();
    if (err.err != DS_FILE_SUCCESS) {
        fprintf(stderr, "driver open: %s\n", ds_file_op_status_error(err.err));
        return 1;
    }

    int fd = open("/mnt/nvme/data.bin", O_RDONLY | O_DIRECT);

    ds_file_handle_t fh;
    err = ds_file_handle_register(&fh, fd);

    size_t buf_size = 1 << 20;  /* 1 MiB */
    void *buf = ds_file_alloc(buf_size);

    ssize_t nread = ds_file_read(fh, buf, buf_size, 0, 0);
    if (nread < 0) {
        fprintf(stderr, "read: %s\n",
                ds_file_op_status_error((ds_file_op_error_t)-nread));
    } else {
        printf("read %zd bytes\n", nread);
    }

    ds_file_free(buf);
    ds_file_handle_deregister(fh);
    close(fd);
    ds_file_driver_close();

    return 0;
}
```

### Buffer offset

The last parameter to `ds_file_read` is a byte offset into the
destination buffer. This exists because GPU device pointers cannot be
dereferenced or offset from host code. Instead of pointer arithmetic,
pass the base pointer and let the backend apply the offset.

```c
/* Read two 4 KiB blocks into different regions of a device buffer. */
ds_file_read(fh, dev_buf, 4096, 0,    0);     /* -> dev_buf[0..4095]    */
ds_file_read(fh, dev_buf, 4096, 4096, 4096);  /* -> dev_buf[4096..8191] */
```

### Error handling

Functions returning `ds_file_error_t` carry both a ds_file error code
and an optional backend-specific code. Functions returning `ssize_t`
(read/write) return the byte count on success or a negated error on
failure.

```c
ds_file_error_t err = ds_file_handle_register(&fh, fd);

if (err.err != DS_FILE_SUCCESS) {
    fprintf(stderr, "%s\n", ds_file_op_status_error(err.err));
}

ssize_t n = ds_file_read(fh, buf, size, offset, 0);
if (n < 0) {
    fprintf(stderr, "read: %s\n",
            ds_file_op_status_error((ds_file_op_error_t)-n));
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
[CIJOE](https://github.com/refenv/cijoe). The target needs an NVMe
device, an NVIDIA GPU (for GDS and aisio tests), GDS support (for
GDS tests), and xNVMe with the `upcie-cuda` backend (for aisio
tests). The xNVMe build dependency is pinned in the tracked
`configs/deps.toml`; `scripts/setup_deps.py` builds and installs it
on the target from that pin.

The `test_sync_read_prep` step writes a pattern file on the mounted
filesystem and each backend test reads it back. The aisio phase runs
last: it builds an extent cache while the filesystem is still
mounted, then unbinds the NVMe kernel driver so the backend can
drive the device directly over PCIe.

1. Copy the example configs and fill in target details:

   ```sh
   cp configs/transport.toml.example configs/transport.toml
   cp configs/test.toml.example configs/test.toml
   ```

   `configs/deps.toml` is tracked in-repo and needs no editing.

2. First-run bootstrap. Sync the tree, install xNVMe on the target,
   build OpenDS:

   ```sh
   python scripts/rsync.py
   python scripts/setup_deps.py   # Only needed once, for the aisio backend.
   python scripts/build.py
   ```

   Iterative loop drops `setup_deps.py`:

   ```sh
   python scripts/rsync.py && python scripts/build.py
   ```

3. Run all test suites:

   ```sh
   python scripts/run_tests.py
   ```

   This runs `tasks/test.yaml`: binds and mounts the NVMe device,
   writes the pattern file, exercises the ref and GDS backends, and
   (if aisio is enabled) caches extents, unbinds the kernel driver,
   and runs the aisio test against the unbound device.
