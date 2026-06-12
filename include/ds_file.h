/*
 * ds_file.h - Accelerator Direct Storage File I/O Interface
 *
 * Vendor-neutral API for direct storage access, structurally compatible
 * with NVIDIA's cuFile API. Applications written against cuFile can be
 * ported by renaming symbols.
 *
 * The reference backend (libopends_ref) uses POSIX pread/pwrite on
 * host buffers and has no external dependencies.
 *
 * Deviations from cuFile:
 *
 *   - Linux only. File handles are plain file descriptors; the
 *     cuFileDescr_t type/union and USERSPACE_FS handle type are
 *     not supported.
 *
 *   - Buffer registration (cuFileBufRegister/Deregister) is offered
 *     alongside backend-owned allocation via ds_file_alloc/ds_file_free.
 *     Callers may allocate their own memory (e.g. cudaMalloc) and call
 *     ds_file_buf_register, or let the backend handle both with
 *     ds_file_alloc.
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

/* clang-format off */
typedef enum ds_file_op_error {
	DS_FILE_SUCCESS                  = 0,
	DS_FILE_DRIVER_NOT_INITIALIZED   = 5001,
	DS_FILE_DRIVER_INVALID_PROPS     = 5002,
	DS_FILE_DRIVER_UNSUPPORTED_LIMIT = 5003,
	DS_FILE_DRIVER_VERSION_MISMATCH  = 5004,
	DS_FILE_DRIVER_VERSION_READ_ERROR = 5005,
	DS_FILE_DRIVER_CLOSING           = 5006,
	DS_FILE_PLATFORM_NOT_SUPPORTED   = 5007,
	DS_FILE_IO_NOT_SUPPORTED         = 5008,
	DS_FILE_DEVICE_NOT_SUPPORTED     = 5009,
	DS_FILE_FS_DRIVER_ERROR          = 5010,
	DS_FILE_DEVICE_DRIVER_ERROR      = 5011,
	DS_FILE_POINTER_INVALID          = 5012,
	DS_FILE_MEMORY_TYPE_INVALID      = 5013,
	DS_FILE_POINTER_RANGE_ERROR      = 5014,
	DS_FILE_CONTEXT_MISMATCH         = 5015,
	DS_FILE_INVALID_MAPPING_SIZE     = 5016,
	DS_FILE_INVALID_MAPPING_RANGE    = 5017,
	DS_FILE_INVALID_FILE_TYPE        = 5018,
	DS_FILE_INVALID_FILE_OPEN_FLAG   = 5019,
	DS_FILE_DIO_NOT_SET              = 5020,
	/* 5021 intentionally unused */
	DS_FILE_INVALID_VALUE            = 5022,
	DS_FILE_MEMORY_ALREADY_REGISTERED = 5023,
	DS_FILE_MEMORY_NOT_REGISTERED    = 5024,
	DS_FILE_PERMISSION_DENIED        = 5025,
	DS_FILE_DRIVER_ALREADY_OPEN      = 5026,
	DS_FILE_HANDLE_NOT_REGISTERED    = 5027,
	DS_FILE_HANDLE_ALREADY_REGISTERED = 5028,
	DS_FILE_DEVICE_NOT_FOUND         = 5029,
	DS_FILE_INTERNAL_ERROR           = 5030,
	DS_FILE_GETNEWFD_FAILED          = 5031,
	/* 5032 intentionally unused */
	DS_FILE_FS_SETUP_ERROR           = 5033,
	DS_FILE_IO_DISABLED              = 5034,
	DS_FILE_BATCH_SUBMIT_FAILED      = 5035,
	DS_FILE_MEMORY_PINNING_FAILED    = 5036,
	DS_FILE_BATCH_FULL               = 5037,
	DS_FILE_ASYNC_NOT_SUPPORTED      = 5038,
	DS_FILE_FS_DIRTY                 = 5039,
	DS_FILE_IO_MAX_ERROR             = 5040,
} ds_file_op_error_t;
/* clang-format on */

typedef struct ds_file_error {
	ds_file_op_error_t err;
	ds_result_t dev_err;
} ds_file_error_t;

#define DS_FILE_BASE_ERR 5000
#define IS_DS_FILE_ERR(err) (abs((err)) > DS_FILE_BASE_ERR)
#define DS_FILE_ERRSTR(err)                                                    \
	ds_file_op_status_error((ds_file_op_error_t)abs((err)))

const char *ds_file_op_status_error(ds_file_op_error_t status);

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
ds_file_error_t ds_file_get_version(unsigned *major, unsigned *minor,
                                    unsigned *patch);
ds_file_error_t ds_file_driver_get_properties(ds_file_drv_props_t *props);
ds_file_error_t
ds_file_driver_set_max_direct_io_size(size_t max_direct_io_size);

/*
 * File handle registration. Each file descriptor must be registered
 * before it can be used with ds_file_read/write. The file must be
 * opened with O_DIRECT. Linux only.
 */
ds_file_error_t ds_file_handle_register(ds_file_handle_t *fh, int fd);
void ds_file_handle_deregister(ds_file_handle_t fh);

/*
 * Re-index the backend's view of the mounted filesystem.
 *
 * Retained for source compatibility and treated as a no-op: the aisio backend's
 * HOMI daemon re-indexes on its own when the filesystem changes, so a caller
 * that sees DS_FILE_FS_DIRTY need only retry. Backends without a filesystem
 * index (ref, gds) were always no-ops here.
 */
ds_file_error_t ds_file_reindex(void);

/*
 * Buffer allocation. Buffers used with ds_file_read/write must be
 * either allocated through ds_file_alloc or registered with
 * ds_file_buf_register so the backend can set up DMA mappings.
 */
void *ds_file_alloc(size_t size);
void ds_file_free(void *buf);

/*
 * Register an externally allocated buffer for use with ds_file_read
 * and ds_file_write. The caller retains ownership of the allocation;
 * deregister before freeing. flags is forwarded to the backend (e.g.
 * cuFileBufRegister flags for gds); the ref and aisio backends ignore
 * it.
 */
ds_file_error_t ds_file_buf_register(const void *buf_base, size_t size,
                                     int flags);
ds_file_error_t ds_file_buf_deregister(const void *buf_base);

/*
 * Synchronous I/O. Returns byte count on success or a negated
 * ds_file_op_error_t on failure. The buf_offset parameter writes
 * into the buffer at an offset, useful for scatter reads into a
 * single allocation.
 */
ssize_t ds_file_read(ds_file_handle_t fh, void *buf_base, size_t size,
                     off_t file_offset, off_t buf_offset);
ssize_t ds_file_write(ds_file_handle_t fh, const void *buf_base, size_t size,
                      off_t file_offset, off_t buf_offset);

#ifdef __cplusplus
}
#endif

#endif /* DS_FILE_H_ */
