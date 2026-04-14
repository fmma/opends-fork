# API comparison: cuFile, hipFile, OpenDS

Reference versions:

- **cuFile**: nvidia-cufile-cu12 1.13.1.3 (pip), CUDA 12
- **hipFile**: ROCm/rocm-systems develop branch, hipFile 0.2.0
- **OpenDS**: current tree (ds_file.h, ds_file_batch.h, ds_file_async.h)

## Type mapping

| cuFile | hipFile | OpenDS | Notes |
|--------|---------|--------|-------|
| `CUresult` | `hipError_t` | `ds_result_t` (`int`) | Backend-specific error code. OpenDS uses a plain int, not a vendor enum. |
| `CUstream` | `hipStream_t` | `ds_stream_t` (`void *`) | OpenDS uses an opaque pointer; no GPU runtime dependency. |
| `CUfileOpError` | `hipFileOpError_t` | `ds_file_op_error_t` | Same numeric values (5001-5039). |
| `CUfileError_t` | `hipFileError_t` | `ds_file_error_t` | Struct with op error + backend error. hipFile has `[[nodiscard]]` in C++17/C23. |
| `CUfileHandle_t` | `hipFileHandle_t` | `ds_file_handle_t` | All three: `void *`. |
| `CUfileDescr_t` | `hipFileDescr_t` | *(none)* | OpenDS takes a plain `int fd` directly. |
| `CUfileDrvProps_t` | `hipFileDriverProps_t` | `ds_file_drv_props_t` | See "Driver properties" below. |
| `CUfileIOParams_t` | `hipFileIOParams_t` | `ds_file_io_params_t` | Batch I/O parameters. |
| `CUfileIOEvents_t` | `hipFileIOEvents_t` | `ds_file_io_events_t` | Batch I/O completion events. |
| `CUfileOpcode_t` | `hipFileOpcode_t` | `ds_file_opcode_t` | Read = 0, Write = 1. |
| `CUfileStatus_t` | `hipFileStatus_t` | `ds_file_status_t` | Same bitmask values. |
| `CUfileBatchMode_t` | `hipFileBatchMode_t` | `ds_file_batch_mode_t` | Single value: 1. |
| `CUfileBatchHandle_t` | `hipFileBatchHandle_t` | `ds_file_batch_handle_t` | All three: `void *`. |
| `off_t` | `hoff_t` | `off_t` | hipFile abstracts offset type for Win32 portability. |
| `CUfileFSOps_t` | `hipFileFSOps_t` | *(none)* | Userspace RDMA FS vtable. Not applicable to OpenDS. |
| `cufileRDMAInfo_t` | `hipFileRDMAInfo_t` | *(none)* | RDMA descriptor. Not applicable to OpenDS. |
| `CUfileDriverStatusFlags_t` | `hipFileDriverStatusFlags_t` | *(none)* | Filesystem support bitfield. |
| `CUfileDriverControlFlags_t` | `hipFileDriverControlFlags_t` | *(none)* | Poll mode / compat mode flags. |
| `CUfileFeatureFlags_t` | `hipFileFeatureFlags_t` | *(none)* | Feature capability bits. |
| `sockaddr_t` | `struct sockaddr` | *(none)* | cuFile typedefs `struct sockaddr`; hipFile uses it directly. |

## Error handling

| cuFile | hipFile | OpenDS | Notes |
|--------|---------|--------|-------|
| `cufileop_status_error()` | `hipFileGetOpErrorString()` | `ds_file_op_status_error()` | cuFile: static inline switch. hipFile/OpenDS: exported function. |
| `IS_CUFILE_ERR(err)` | `IS_HIPFILE_ERR(err)` | `IS_DS_FILE_ERR(err)` | Macro: `abs(err) > BASE_ERR`. |
| `CUFILE_ERRSTR(err)` | `HIPFILE_ERRSTR(err)` | `DS_FILE_ERRSTR(err)` | Macro wrapping error string function. |
| `IS_CUDA_ERR(status)` | `IS_HIP_DRV_ERR(err)` | *(none)* | Check if backend driver error. |
| `CU_FILE_CUDA_ERR(status)` | `HIP_DRV_ERR(err)` | *(none)* | Extract backend error from struct. |

Error enum values are identical across all three (5001-5039, with 5021 and 5032 unused). Naming differs:

| Value | cuFile | hipFile | OpenDS |
|-------|--------|---------|--------|
| 5010 | `CU_FILE_NVFS_DRIVER_ERROR` | `hipFileDriverError` | `DS_FILE_FS_DRIVER_ERROR` |
| 5011 | `CU_FILE_CUDA_DRIVER_ERROR` | `hipFileHipDriverError` | `DS_FILE_DEVICE_DRIVER_ERROR` |
| 5012 | `CU_FILE_CUDA_POINTER_INVALID` | `hipFileHipPointerInvalid` | `DS_FILE_POINTER_INVALID` |
| 5013 | `CU_FILE_CUDA_MEMORY_TYPE_INVALID` | `hipFileHipMemoryTypeInvalid` | `DS_FILE_MEMORY_TYPE_INVALID` |
| 5014 | `CU_FILE_CUDA_POINTER_RANGE_ERROR` | `hipFileHipPointerRangeError` | `DS_FILE_POINTER_RANGE_ERROR` |
| 5015 | `CU_FILE_CUDA_CONTEXT_MISMATCH` | `hipFileHipContextMismatch` | `DS_FILE_CONTEXT_MISMATCH` |
| 5033 | `CU_FILE_NVFS_SETUP_ERROR` | `hipFileDriverSetupError` | `DS_FILE_FS_SETUP_ERROR` |
| 5036 | `CU_FILE_GPU_MEMORY_PINNING_FAILED` | `hipFileGPUMemoryPinningFailed` | `DS_FILE_MEMORY_PINNING_FAILED` |

OpenDS strips the CUDA/NVFS/GPU prefixes, using vendor-neutral names ("device", "filesystem").

## Driver properties

| cuFile field | hipFile field | OpenDS field | Notes |
|--------------|---------------|--------------|-------|
| `nvfs.major_version` | `nvfs.major_version` | `major_version` | OpenDS: flat struct, no `nvfs` nesting. |
| `nvfs.minor_version` | `nvfs.minor_version` | `minor_version` | |
| `nvfs.poll_thresh_size` | `nvfs.poll_thresh_size` | *(none)* | |
| `nvfs.max_direct_io_size` | `nvfs.max_direct_io_size` | `max_direct_io_size` | |
| `nvfs.dstatusflags` | `nvfs.driver_status_flags` | *(none)* | |
| `nvfs.dcontrolflags` | `nvfs.driver_control_flags` | *(none)* | |
| `fflags` | `feature_flags` | *(none)* | |
| `max_device_cache_size` | `max_device_cache_size` | *(none)* | |
| `per_buffer_cache_size` | `per_buffer_cache_size` | *(none)* | |
| `max_device_pinned_mem_size` | `max_device_pinned_mem_size` | *(none)* | |
| `max_batch_io_size` | `max_batch_io_count` | `max_batch_io_size` | hipFile renames to `max_batch_io_count`. |
| `max_batch_io_timeout_msecs` | `max_batch_io_timeout_msecs` | `max_batch_io_timeout_msecs` | |

cuFile uses `unsigned int` for sizes; hipFile widens several to `uint64_t`. OpenDS uses `size_t` for `max_direct_io_size` and `unsigned int` for the rest.

OpenDS omits all flag-based fields (status, control, feature flags), poll threshold, cache sizes, and pinned memory size. The struct is flat with no `nvfs` sub-struct.

## File handle registration

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileHandleRegister(CUfileHandle_t *fh, CUfileDescr_t *descr)` | `hipFileHandleRegister(hipFileHandle_t *fh, hipFileDescr_t *descr)` | `ds_file_handle_register(ds_file_handle_t *fh, int fd)` |
| `cuFileHandleDeregister(CUfileHandle_t fh)` | `hipFileHandleDeregister(hipFileHandle_t fh)` | `ds_file_handle_deregister(ds_file_handle_t fh)` |

cuFile and hipFile use a descriptor struct containing a handle type enum (`OPAQUE_FD`, `OPAQUE_WIN32`, `USERSPACE_FS`), a union for the OS handle, and an optional `FSOps` vtable for userspace RDMA filesystems.

OpenDS takes a plain `int fd`. No descriptor struct, no handle type enum, no Win32 support, no userspace FS vtable. Linux only.

## Buffer management

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileBufRegister(ptr, len, flags)` | `hipFileBufRegister(ptr, len, flags)` | *(none)* |
| `cuFileBufDeregister(ptr)` | `hipFileBufDeregister(ptr)` | *(none)* |
| *(user allocates via cudaMalloc)* | *(user allocates via hipMalloc)* | `ds_file_alloc(size)` |
| *(user frees via cudaFree)* | *(user frees via hipFree)* | `ds_file_free(buf)` |

cuFile and hipFile separate allocation (GPU runtime) from registration (cuFile/hipFile library). The user allocates with `cudaMalloc`/`hipMalloc` and then registers the pointer for DMA.

OpenDS owns both allocation and deallocation. `ds_file_alloc()` replaces both `cudaMalloc` + `cuFileBufRegister`. This is necessary because the backend must control the allocation to set up DMA mappings (e.g. through xNVMe). There is no flags parameter.

## Synchronous I/O

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileRead(fh, ptr, size, file_off, buf_off)` | `hipFileRead(fh, ptr, size, file_off, buf_off)` | `ds_file_read(fh, ptr, size, file_off, buf_off)` |
| `cuFileWrite(fh, ptr, size, file_off, buf_off)` | `hipFileWrite(fh, ptr, size, file_off, buf_off)` | `ds_file_write(fh, ptr, size, file_off, buf_off)` |

Return type is `ssize_t` in all three. Return conventions are identical: byte count on success, -1 for `errno`-reported errors, negated op error enum for library errors.

Offset types: cuFile and OpenDS use `off_t`; hipFile uses `hoff_t` (typedef to `off_t` on Linux, `__int64` on Win32).

## Driver lifecycle

| cuFile | hipFile | OpenDS | Notes |
|--------|---------|--------|-------|
| `cuFileDriverOpen()` | `hipFileDriverOpen()` | `ds_file_driver_open()` | |
| `cuFileDriverClose()` | `hipFileDriverClose()` | `ds_file_driver_close()` | cuFile has a `_v2` alias via `#define`. |
| `cuFileUseCount()` → `long` | `hipFileUseCount()` → `int64_t` | `ds_file_use_count()` → `long` | |
| `cuFileDriverGetProperties()` | `hipFileDriverGetProperties()` | `ds_file_driver_get_properties()` | |
| `cuFileDriverSetPollMode(poll, size)` | `hipFileDriverSetPollMode(poll, size)` | *(none)* | |
| `cuFileDriverSetMaxDirectIOSize(size)` | `hipFileDriverSetMaxDirectIOSize(size)` | `ds_file_driver_set_max_direct_io_size(size)` | |
| `cuFileDriverSetMaxCacheSize(size)` | `hipFileDriverSetMaxCacheSize(size)` | *(none)* | |
| `cuFileDriverSetMaxPinnedMemSize(size)` | `hipFileDriverSetMaxPinnedMemSize(size)` | *(none)* | |

OpenDS omits poll mode, cache size, and pinned memory size controls.

## Version query

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileGetVersion(int *version)` | `hipFileGetVersion(unsigned *major, unsigned *minor, unsigned *patch)` | `ds_file_get_version(unsigned *major, unsigned *minor, unsigned *patch)` |

cuFile returns a packed integer: `1000 * major + 10 * minor`. hipFile and OpenDS use three separate output parameters. NULL parameters are allowed (ignored).

## Batch I/O

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileBatchIOSetUp(handle, nr)` | `hipFileBatchIOSetUp(handle, max_nr)` | `ds_file_batch_io_setup(handle, nr)` |
| `cuFileBatchIOSubmit(handle, nr, params, flags)` | `hipFileBatchIOSubmit(handle, nr, params, flags)` | `ds_file_batch_io_submit(handle, nr, params, flags)` |
| `cuFileBatchIOGetStatus(handle, min, nr, events, timeout)` | `hipFileBatchIOGetStatus(handle, min, nr, events, timeout)` | `ds_file_batch_io_get_status(handle, min, nr, events, timeout)` |
| `cuFileBatchIOCancel(handle)` | `hipFileBatchIOCancel(handle)` | `ds_file_batch_io_cancel(handle)` |
| `cuFileBatchIODestroy(handle)` | `hipFileBatchIODestroy(handle)` | `ds_file_batch_io_destroy(handle)` |

Semantics and signatures are identical across all three. Naming convention differs (CamelCase vs snake_case).

Batch offset types: cuFile uses `off_t`; hipFile uses `int64_t`; OpenDS uses `off_t`.

## Async / stream I/O

| cuFile | hipFile | OpenDS |
|--------|---------|--------|
| `cuFileReadAsync(fh, ptr, size_p, foff_p, boff_p, read_p, stream)` | `hipFileReadAsync(fh, ptr, size_p, foff_p, boff_p, read_p, stream)` | `ds_file_read_async(fh, ptr, size_p, foff_p, boff_p, read_p, stream)` |
| `cuFileWriteAsync(fh, ptr, size_p, foff_p, boff_p, written_p, stream)` | `hipFileWriteAsync(fh, ptr, size_p, foff_p, boff_p, written_p, stream)` | `ds_file_write_async(fh, ptr, size_p, foff_p, boff_p, written_p, stream)` |
| `cuFileStreamRegister(stream, flags)` | `hipFileStreamRegister(stream, flags)` | `ds_file_stream_register(stream, flags)` |
| `cuFileStreamDeregister(stream)` | `hipFileStreamDeregister(stream)` | `ds_file_stream_deregister(stream)` |

All three use pointer-indirect parameters for deferred reads at stream execution time. Stream type differs (`CUstream` vs `hipStream_t` vs `ds_stream_t`).

Stream registration flags:

| cuFile | hipFile | OpenDS | Value |
|--------|---------|--------|-------|
| `CU_FILE_STREAM_FIXED_BUF_OFFSET` | `HIPFILE_STREAM_FIXED_BUF_OFFSET` | *(none)* | 1 |
| `CU_FILE_STREAM_FIXED_FILE_OFFSET` | `HIPFILE_STREAM_FIXED_FILE_OFFSET` | *(none)* | 2 |
| `CU_FILE_STREAM_FIXED_FILE_SIZE` | `HIPFILE_STREAM_FIXED_FILE_SIZE` | *(none)* | 4 |
| `CU_FILE_STREAM_PAGE_ALIGNED_INPUTS` | `HIPFILE_STREAM_PAGE_ALIGNED_INPUTS` | *(none)* | 8 |
| *(none)* | `HIPFILE_STREAM_FLAGS_MASK` | *(none)* | 0xf |

OpenDS does not define stream flag constants.

## RDMA

cuFile and hipFile both define:

- An RDMA info struct (`cufileRDMAInfo_t` / `hipFileRDMAInfo_t`).
- Registration flags (`CU_FILE_RDMA_REGISTER`, `CU_FILE_RDMA_RELAXED_ORDERING`).
- A filesystem operations vtable (`CUfileFSOps_t` / `hipFileFSOps_t`) for userspace RDMA filesystems.

OpenDS has no RDMA support.

## Configuration parameters (hipFile only)

hipFile adds a configuration API with no cuFile or OpenDS equivalent:

- `hipFileGetParameterSizeT()` / `hipFileSetParameterSizeT()` (12 parameters)
- `hipFileGetParameterBool()` / `hipFileSetParameterBool()` (12 parameters)
- `hipFileGetParameterString()` / `hipFileSetParameterString()` (3 parameters)

cuFile uses a JSON configuration file (`cufile.json`) instead of a programmatic API. OpenDS has neither.

## Filesystem support enums (hipFile addition)

hipFile adds `hipFileScatefsSupported = 12` to the driver status flags. cuFile stops at value 11.

## Summary of OpenDS deviations from cuFile

1. **No descriptor struct.** `ds_file_handle_register()` takes `int fd` directly. No `CUfileDescr_t`, no handle type enum, no Win32, no userspace FS vtable.

2. **Backend-owned allocation replaces buffer registration.** `ds_file_alloc()` / `ds_file_free()` replace `cuFileBufRegister()` / `cuFileBufDeregister()`. The backend must own allocations to set up DMA mappings.

3. **Flat driver properties struct.** No `nvfs` sub-struct. Omits poll threshold, status/control/feature flags, cache sizes, and pinned memory size.

4. **Omitted driver controls.** No `SetPollMode`, `SetMaxCacheSize`, or `SetMaxPinnedMemSize`.

5. **No RDMA types.** No `cufileRDMAInfo_t`, `CUfileFSOps_t`, `sockaddr_t`, or RDMA registration flags.

6. **Partial error macros.** `IS_DS_FILE_ERR` and `DS_FILE_ERRSTR` are provided. Backend-specific error macros (`IS_CUDA_ERR`, `CU_FILE_CUDA_ERR`) are not applicable.

7. **No stream flag constants.** `CU_FILE_STREAM_FIXED_*` and `CU_FILE_STREAM_PAGE_ALIGNED_INPUTS` are not defined.

8. **Vendor-neutral naming.** Error codes strip CUDA/NVFS/GPU prefixes. "CUDA driver error" becomes "device driver error", "NVFS driver error" becomes "filesystem driver error".

9. **Generic backend error type.** `ds_result_t` is `int` rather than `CUresult`, decoupling from any GPU vendor runtime.

10. **Generic stream type.** `ds_stream_t` is `void *` rather than `CUstream`, allowing any backend to provide its own stream implementation.
