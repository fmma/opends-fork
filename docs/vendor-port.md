# GPU vendor portability of the aisio backend

The aisio backend reads NVMe straight into GPU memory over xNVMe P2P DMA.
Its only GPU dependency is a narrow runtime surface, isolated behind an
interface so that supporting a second vendor (AMD via ROCm/HIP, or any
other) is writing one implementation file plus build wiring, not editing
the NVMe and extent logic.

This document records the interface and the checklist for adding a
vendor. Only the CUDA implementation is built today.

## The interface

`src/ds_gpu.h` defines `struct ds_gpu_ops`, the function-pointer table a
vendor implements, and declares the active-ops pointer:

```c
extern const struct ds_gpu_ops *const ds_gpu;
```

The NVMe and extent code calls only through `ds_gpu` (for example
`ds_gpu->copy(...)`, `ds_gpu->stream_write_value32(...)`), never a vendor
symbol. The ops table covers: context capture and thread binding, host
pinned-mapped allocation, host/device copy, the per-stream stream-value
gate ops, the bounce copy kernel launch, and the xNVMe P2P backend string
for the vendor.

A vendor supplies one translation unit (for CUDA, `src/ds_gpu_cuda.c`)
that defines a `struct ds_gpu_ops` instance and binds `ds_gpu` to it. The
build compiles exactly one such file, so `ds_gpu` resolves at link time
with no runtime branching. Device buffer allocation is not in the table:
it is delegated to xNVMe (`xnvme_buf_alloc`), so the backend never
allocates GPU memory itself.

The chokepoint is enforceable: no `cu*` / `cuda*` symbol appears in
`opends_aisio.c`, `ds_bounce_ctx.c`, or `ds_stream_map.h`.

### Interface types and contracts

Interface types are vendor-uniform so the ops ABI is fixed:

- `ds_gpu_ctx_t` and `ds_gpu_stream_t` are opaque handles (`void *`).
- `ds_gpu_devptr_t` is a GPU device address as a plain integer
  (`uint64_t`). Each vendor converts to and from its native pointer at
  the boundary: identity for CUDA's `CUdeviceptr`, a cast for HIP's
  `void *`. Callers do ordinary arithmetic on it (`ds_gpu_devptr_add`).
  This keeps the device-pointer representation difference contained in
  each vendor's implementation rather than spread across the NVMe code.

One semantic contract: `ds_gpu_ops::bind_thread` means "make this
thread's GPU work target the captured context", not a specific
primitive. CUDA implements it with `cuCtxSetCurrent`; HIP would use
`hipSetDevice`, since its driver context model differs.

### Async ordering

The async path orders the I/O thread against the user's stream with a
per-stream gate word: the stream's `stream_write_value32(2*seq)` publishes
arrival, and the I/O thread's plain host store of `2*seq+1` releases the
`stream_wait_value32_geq(2*seq+1)` once the I/O is done. Only the two
stream enqueue ops are vendor-specific; the NVMe code owns the word array
(a single mapped allocation sliced per stream) and does the host-side poll
and release. A vendor whose stream wait-value is unusable would rework
this async ordering in the NVMe code, not just its `ds_gpu_<vendor>.c`.

### The device kernel

`src/ds_bounce_kernel.cu` is compiled by the GPU compiler and does not
include `ds_gpu.h`. It exports `ds_gpu_bounce_launch` (the function the
CUDA ops table's `bounce` field binds to) and selects its runtime
(`cuda_runtime.h` vs `hip/hip_runtime.h`), its stream type, and its
launch-error call on the `DS_GPU_HIP` build macro. The kernel body and
the `<<<>>>` launch are already valid under hipcc unchanged.

### Build selection

`meson_options.txt` exposes `gpu_backend` (default `cuda`). `meson.build`
turns it into the `ds_gpu_<vendor>.c` source, the linked GPU deps, the
system include dirs, and the bounce-kernel language and compile args,
behind one branch. Only `cuda` is wired up; any other value stops
configuration with a pointer here. The build then has the same single
seam as the code: one option and one branch, not edits scattered across
the aisio rules.

## Adding a vendor (deferred)

None of the following is implemented yet. The interface is shaped to
accommodate it.

- **V1.** `src/ds_gpu_hip.h`/`.c` implementing `struct ds_gpu_ops` and
  binding `ds_gpu`, plus the HIP branch of `ds_bounce_kernel.cu`.
- **V2.** The `gpu_backend` meson option already selects the
  `ds_gpu_<vendor>.c` source, the vendor deps, the kernel language, and
  the kernel args. ROCm is a new `elif gpu_backend == 'rocm'` branch in
  `meson.build` (its deps, `-DDS_GPU_HIP` in `gpu_kernel_args`, ROCm
  include dirs) plus setting the vendor's `xnvme_be` (`"upcie-amd"`) in
  `ds_gpu_rocm.c`.
- **V3.** Python: make the context-preservation guard and the loader
  vendor-aware (keyed on the backend name). The `__cuda_array_interface__`
  buffer view already covers ROCm tensors.
- **V4.** Tests: a HIP variant of `tests/test_cuda_common.h` behind the
  same interface shape.
- **V5.** Two spikes on real AMD hardware before V1 is trusted:
  1. `hipStreamWaitValue32` (GEQ) actually waits on host-written mapped
     memory updated by another thread, on the target gfx arch. Query the
     capability attribute; do not trust return codes. If it fails, the
     fallback (events or semaphores) means reworking the async ordering in
     the NVMe code, since the gate is built from the stream-value ops
     rather than hidden behind one interface call.
  2. Thread-to-device binding semantics (`hipSetDevice` vs
     `hipCtxSetCurrent`) for the I/O thread.

External prerequisites, neither of which this codebase owns: an xNVMe
`upcie-amd` (or equivalent) P2P backend, and AMD hardware to validate on.

## Why not libdrm

dma-buf, not libdrm, is the cross-vendor primitive, and it already sits
below this backend inside xNVMe's upcie P2P registration. libdrm is a
per-vendor DRM ioctl wrapper with no notion of streams or kernel launch,
and the NVIDIA stack does not allocate through it, so a libdrm path would
cover AMD only and still leave CUDA in place. The interface abstracts the
compute runtime; xNVMe owns the dma-buf export.
