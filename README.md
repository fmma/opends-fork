# OpenGDS

Open source accelerator direct storage. Provides a vendor-neutral
ds_file API for reading files directly into accelerator memory.

The reference backend uses POSIX `pread`/`pwrite` on host buffers and
has no external dependencies. It serves as a baseline for correctness
testing and as a template for hardware-specific backends.

## ds_file API

### Basic read

Reads must be LBA-aligned (both offset and size). The file must be
opened with `O_DIRECT`. Unaligned reads via bounce buffers are planned
but not yet implemented.

```c
#include <opengds.h>
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

Requires [meson](https://mesonbuild.com) and a C11 compiler.

```sh
meson setup build
meson compile -C build
```

## Testing

Run the reference backend smoke test:

```sh
./build/smoke_ref
```

The smoke test creates a temporary file and exercises the full ds_file
lifecycle (driver open, handle register, buffer alloc, write, read,
verify, deregister, close) against the reference backend.
