/*
 * ds_file.h - Accelerator Direct Storage File I/O Interface
 *
 * Vendor-neutral API for direct storage access, structurally compatible
 * with NVIDIA's cuFile API. Applications written against cuFile can be
 * ported by renaming symbols.
 *
 * The reference backend (libopengds_ref) uses POSIX pread/pwrite on
 * host buffers and has no external dependencies.
 *
 * Deviations from cuFile:
 *
 *   - Linux only. File handles are plain file descriptors; the
 *     cuFileDescr_t type/union and USERSPACE_FS handle type are
 *     not supported.
 *
 *   - Buffer registration (cuFileBufRegister/Deregister) is replaced
 *     by ds_file_alloc/ds_file_free. The backend must own allocations
 *     to set up DMA mappings through xNVMe.
 *
 * Error reporting:
 *
 *   Functions returning ds_file_error_t use a structured error with
 *   both a ds_file error code and an optional backend-specific code.
 *
 *   Functions returning ssize_t (read/write) return the byte count on
 *   success or a negated ds_file_op_error_t on failure.
 */
#ifndef DS_FILE_H_
#define DS_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <sys/types.h>

typedef int ds_result_t;

#define DS_FILEOP_BASE_ERR 5000

typedef enum ds_file_op_error {
	DS_FILE_SUCCESS                    = 0,
	DS_FILE_DRIVER_NOT_INITIALIZED     = DS_FILEOP_BASE_ERR + 1,
	DS_FILE_DRIVER_INVALID_PROPS       = DS_FILEOP_BASE_ERR + 2,
	DS_FILE_DRIVER_UNSUPPORTED_LIMIT   = DS_FILEOP_BASE_ERR + 3,
	DS_FILE_DRIVER_VERSION_MISMATCH    = DS_FILEOP_BASE_ERR + 4,
	DS_FILE_DRIVER_VERSION_READ_ERROR  = DS_FILEOP_BASE_ERR + 5,
	DS_FILE_DRIVER_CLOSING             = DS_FILEOP_BASE_ERR + 6,
	DS_FILE_PLATFORM_NOT_SUPPORTED     = DS_FILEOP_BASE_ERR + 7,
	DS_FILE_IO_NOT_SUPPORTED           = DS_FILEOP_BASE_ERR + 8,
	DS_FILE_DEVICE_NOT_SUPPORTED       = DS_FILEOP_BASE_ERR + 9,
	DS_FILE_FS_DRIVER_ERROR            = DS_FILEOP_BASE_ERR + 10,
	DS_FILE_DEVICE_DRIVER_ERROR        = DS_FILEOP_BASE_ERR + 11,
	DS_FILE_POINTER_INVALID            = DS_FILEOP_BASE_ERR + 12,
	DS_FILE_MEMORY_TYPE_INVALID        = DS_FILEOP_BASE_ERR + 13,
	DS_FILE_POINTER_RANGE_ERROR        = DS_FILEOP_BASE_ERR + 14,
	DS_FILE_CONTEXT_MISMATCH           = DS_FILEOP_BASE_ERR + 15,
	DS_FILE_INVALID_MAPPING_SIZE       = DS_FILEOP_BASE_ERR + 16,
	DS_FILE_INVALID_MAPPING_RANGE      = DS_FILEOP_BASE_ERR + 17,
	DS_FILE_INVALID_FILE_TYPE          = DS_FILEOP_BASE_ERR + 18,
	DS_FILE_INVALID_FILE_OPEN_FLAG     = DS_FILEOP_BASE_ERR + 19,
	DS_FILE_DIO_NOT_SET                = DS_FILEOP_BASE_ERR + 20,
	DS_FILE_INVALID_VALUE              = DS_FILEOP_BASE_ERR + 22,
	DS_FILE_MEMORY_ALREADY_REGISTERED  = DS_FILEOP_BASE_ERR + 23,
	DS_FILE_MEMORY_NOT_REGISTERED      = DS_FILEOP_BASE_ERR + 24,
	DS_FILE_PERMISSION_DENIED          = DS_FILEOP_BASE_ERR + 25,
	DS_FILE_DRIVER_ALREADY_OPEN        = DS_FILEOP_BASE_ERR + 26,
	DS_FILE_HANDLE_NOT_REGISTERED      = DS_FILEOP_BASE_ERR + 27,
	DS_FILE_HANDLE_ALREADY_REGISTERED  = DS_FILEOP_BASE_ERR + 28,
	DS_FILE_DEVICE_NOT_FOUND           = DS_FILEOP_BASE_ERR + 29,
	DS_FILE_INTERNAL_ERROR             = DS_FILEOP_BASE_ERR + 30,
	DS_FILE_GETNEWFD_FAILED            = DS_FILEOP_BASE_ERR + 31,
	DS_FILE_FS_SETUP_ERROR             = DS_FILEOP_BASE_ERR + 33,
	DS_FILE_IO_DISABLED                = DS_FILEOP_BASE_ERR + 34,
	DS_FILE_BATCH_SUBMIT_FAILED        = DS_FILEOP_BASE_ERR + 35,
	DS_FILE_MEMORY_PINNING_FAILED      = DS_FILEOP_BASE_ERR + 36,
	DS_FILE_BATCH_FULL                 = DS_FILEOP_BASE_ERR + 37,
	DS_FILE_ASYNC_NOT_SUPPORTED        = DS_FILEOP_BASE_ERR + 38,
	DS_FILE_IO_MAX_ERROR               = DS_FILEOP_BASE_ERR + 39,
} ds_file_op_error_t;

typedef struct ds_file_error {
	ds_file_op_error_t err;
	ds_result_t dev_err;
} ds_file_error_t;

static inline const char *
ds_file_op_status_error(ds_file_op_error_t status)
{
	switch (status) {
	case DS_FILE_SUCCESS:                    return "ds_file success";
	case DS_FILE_DRIVER_NOT_INITIALIZED:     return "driver is not loaded";
	case DS_FILE_DRIVER_INVALID_PROPS:       return "invalid property";
	case DS_FILE_DRIVER_UNSUPPORTED_LIMIT:   return "property range error";
	case DS_FILE_DRIVER_VERSION_MISMATCH:    return "driver version mismatch";
	case DS_FILE_DRIVER_VERSION_READ_ERROR:  return "driver version read error";
	case DS_FILE_DRIVER_CLOSING:             return "driver shutdown in progress";
	case DS_FILE_PLATFORM_NOT_SUPPORTED:     return "direct storage not supported on current platform";
	case DS_FILE_IO_NOT_SUPPORTED:           return "direct storage not supported on current file";
	case DS_FILE_DEVICE_NOT_SUPPORTED:       return "direct storage not supported on current device";
	case DS_FILE_FS_DRIVER_ERROR:            return "filesystem driver ioctl error";
	case DS_FILE_DEVICE_DRIVER_ERROR:        return "device driver API error";
	case DS_FILE_POINTER_INVALID:            return "invalid device pointer";
	case DS_FILE_MEMORY_TYPE_INVALID:        return "invalid pointer memory type";
	case DS_FILE_POINTER_RANGE_ERROR:        return "pointer range exceeds allocated address range";
	case DS_FILE_CONTEXT_MISMATCH:           return "device context mismatch";
	case DS_FILE_INVALID_MAPPING_SIZE:       return "access beyond maximum pinned size";
	case DS_FILE_INVALID_MAPPING_RANGE:      return "access beyond mapped size";
	case DS_FILE_INVALID_FILE_TYPE:          return "unsupported file type";
	case DS_FILE_INVALID_FILE_OPEN_FLAG:     return "unsupported file open flags";
	case DS_FILE_DIO_NOT_SET:                return "fd direct IO not set";
	case DS_FILE_INVALID_VALUE:              return "invalid arguments";
	case DS_FILE_MEMORY_ALREADY_REGISTERED:  return "device pointer already registered";
	case DS_FILE_MEMORY_NOT_REGISTERED:      return "device pointer lookup failure";
	case DS_FILE_PERMISSION_DENIED:          return "driver or file access error";
	case DS_FILE_DRIVER_ALREADY_OPEN:        return "driver is already open";
	case DS_FILE_HANDLE_NOT_REGISTERED:      return "file descriptor is not registered";
	case DS_FILE_HANDLE_ALREADY_REGISTERED:  return "file descriptor is already registered";
	case DS_FILE_DEVICE_NOT_FOUND:           return "device not found";
	case DS_FILE_INTERNAL_ERROR:             return "internal error";
	case DS_FILE_GETNEWFD_FAILED:            return "failed to obtain new file descriptor";
	case DS_FILE_FS_SETUP_ERROR:             return "filesystem driver initialization error";
	case DS_FILE_IO_DISABLED:                return "direct storage disabled by config on current file";
	case DS_FILE_BATCH_SUBMIT_FAILED:        return "failed to submit batch operation";
	case DS_FILE_MEMORY_PINNING_FAILED:      return "failed to allocate pinned device memory";
	case DS_FILE_BATCH_FULL:                 return "queue full for batch operation";
	case DS_FILE_ASYNC_NOT_SUPPORTED:        return "stream operation not supported";
	case DS_FILE_IO_MAX_ERROR:               return "max error";
	default:                                  return "unknown ds_file error";
	}
}

typedef struct ds_file_drv_props {
	unsigned int major_version;
	unsigned int minor_version;
	size_t max_direct_io_size;
	unsigned int max_batch_io_size;
	unsigned int max_batch_io_timeout_msecs;
} ds_file_drv_props_t;

typedef void *ds_file_handle_t;

/*
 * Driver lifecycle. Call ds_file_driver_open() once before any other
 * ds_file operation. Call ds_file_driver_close() to release all
 * resources when done.
 */
ds_file_error_t ds_file_driver_open(void);
ds_file_error_t ds_file_driver_close(void);
long ds_file_use_count(void);
ds_file_error_t ds_file_get_version(int *version);
ds_file_error_t ds_file_driver_get_properties(ds_file_drv_props_t *props);
ds_file_error_t ds_file_driver_set_max_direct_io_size(size_t max_direct_io_size);

/*
 * File handle registration. Each file descriptor must be registered
 * before it can be used with ds_file_read/write. The file must be
 * opened with O_DIRECT. Linux only.
 */
ds_file_error_t ds_file_handle_register(ds_file_handle_t *fh, int fd);
void ds_file_handle_deregister(ds_file_handle_t fh);

/*
 * Buffer allocation. Buffers used with ds_file_read/write must be
 * allocated through ds_file_alloc so the backend can set up DMA
 * mappings.
 */
void *ds_file_alloc(size_t size);
void ds_file_free(void *buf);

/*
 * Synchronous I/O. Returns byte count on success or a negated
 * ds_file_op_error_t on failure. The buf_offset parameter writes
 * into the buffer at an offset, useful for scatter reads into a
 * single allocation.
 */
ssize_t ds_file_read(ds_file_handle_t fh, void *buf_base,
                      size_t size, off_t file_offset, off_t buf_offset);
ssize_t ds_file_write(ds_file_handle_t fh, const void *buf_base,
                       size_t size, off_t file_offset, off_t buf_offset);

#ifdef __cplusplus
}
#endif

#endif /* DS_FILE_H_ */
