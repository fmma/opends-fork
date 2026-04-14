/*
 * cuFile API — preprocessed interface
 *
 * Source: nvidia-cufile-cu12 1.13.1.3 (pip package)
 * Header: cufile.h, preprocessed with cpp -P -I/usr/include
 * Copyright 1993-2023 NVIDIA Corporation. All rights reserved.
 *
 * This file is a reference copy of the NVIDIA cuFile public API surface
 * after C preprocessing (X-macros expanded, comments stripped).
 * It is not compiled as part of this project.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __CUFILE_H_
#define __CUFILE_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Forward declarations for CUDA types used by the API */
typedef int CUresult;
typedef void *CUstream;

/* --------------------------------------------------------------------------
 *  Error handling
 * -------------------------------------------------------------------------- */

#define CUFILEOP_BASE_ERR 5000

/* clang-format off */
typedef enum CUfileOpError {
	CU_FILE_SUCCESS                   = 0,
	CU_FILE_DRIVER_NOT_INITIALIZED    = 5001,
	CU_FILE_DRIVER_INVALID_PROPS      = 5002,
	CU_FILE_DRIVER_UNSUPPORTED_LIMIT  = 5003,
	CU_FILE_DRIVER_VERSION_MISMATCH   = 5004,
	CU_FILE_DRIVER_VERSION_READ_ERROR = 5005,
	CU_FILE_DRIVER_CLOSING            = 5006,
	CU_FILE_PLATFORM_NOT_SUPPORTED    = 5007,
	CU_FILE_IO_NOT_SUPPORTED          = 5008,
	CU_FILE_DEVICE_NOT_SUPPORTED      = 5009,
	CU_FILE_NVFS_DRIVER_ERROR         = 5010,
	CU_FILE_CUDA_DRIVER_ERROR         = 5011,
	CU_FILE_CUDA_POINTER_INVALID      = 5012,
	CU_FILE_CUDA_MEMORY_TYPE_INVALID  = 5013,
	CU_FILE_CUDA_POINTER_RANGE_ERROR  = 5014,
	CU_FILE_CUDA_CONTEXT_MISMATCH     = 5015,
	CU_FILE_INVALID_MAPPING_SIZE      = 5016,
	CU_FILE_INVALID_MAPPING_RANGE     = 5017,
	CU_FILE_INVALID_FILE_TYPE         = 5018,
	CU_FILE_INVALID_FILE_OPEN_FLAG    = 5019,
	CU_FILE_DIO_NOT_SET               = 5020,
	/* 5021 intentionally unused */
	CU_FILE_INVALID_VALUE             = 5022,
	CU_FILE_MEMORY_ALREADY_REGISTERED = 5023,
	CU_FILE_MEMORY_NOT_REGISTERED     = 5024,
	CU_FILE_PERMISSION_DENIED         = 5025,
	CU_FILE_DRIVER_ALREADY_OPEN       = 5026,
	CU_FILE_HANDLE_NOT_REGISTERED     = 5027,
	CU_FILE_HANDLE_ALREADY_REGISTERED = 5028,
	CU_FILE_DEVICE_NOT_FOUND          = 5029,
	CU_FILE_INTERNAL_ERROR            = 5030,
	CU_FILE_GETNEWFD_FAILED           = 5031,
	/* 5032 intentionally unused */
	CU_FILE_NVFS_SETUP_ERROR          = 5033,
	CU_FILE_IO_DISABLED               = 5034,
	CU_FILE_BATCH_SUBMIT_FAILED       = 5035,
	CU_FILE_GPU_MEMORY_PINNING_FAILED = 5036,
	CU_FILE_BATCH_FULL                = 5037,
	CU_FILE_ASYNC_NOT_SUPPORTED       = 5038,
	CU_FILE_IO_MAX_ERROR              = 5039,
} CUfileOpError;
/* clang-format on */

/* clang-format off */
static inline const char *
cufileop_status_error(CUfileOpError status)
{
	switch (status) {
	case CU_FILE_SUCCESS:                   return "cufile success";
	case CU_FILE_DRIVER_NOT_INITIALIZED:    return "nvidia-fs driver is not loaded";
	case CU_FILE_DRIVER_INVALID_PROPS:      return "invalid property";
	case CU_FILE_DRIVER_UNSUPPORTED_LIMIT:  return "property range error";
	case CU_FILE_DRIVER_VERSION_MISMATCH:   return "nvidia-fs driver version mismatch";
	case CU_FILE_DRIVER_VERSION_READ_ERROR: return "nvidia-fs driver version read error";
	case CU_FILE_DRIVER_CLOSING:            return "driver shutdown in progress";
	case CU_FILE_PLATFORM_NOT_SUPPORTED:    return "GPUDirect Storage not supported on current platform";
	case CU_FILE_IO_NOT_SUPPORTED:          return "GPUDirect Storage not supported on current file";
	case CU_FILE_DEVICE_NOT_SUPPORTED:      return "GPUDirect Storage not supported on current GPU";
	case CU_FILE_NVFS_DRIVER_ERROR:         return "nvidia-fs driver ioctl error";
	case CU_FILE_CUDA_DRIVER_ERROR:         return "CUDA Driver API error";
	case CU_FILE_CUDA_POINTER_INVALID:      return "invalid device pointer";
	case CU_FILE_CUDA_MEMORY_TYPE_INVALID:  return "invalid pointer memory type";
	case CU_FILE_CUDA_POINTER_RANGE_ERROR:  return "pointer range exceeds allocated address range";
	case CU_FILE_CUDA_CONTEXT_MISMATCH:     return "cuda context mismatch";
	case CU_FILE_INVALID_MAPPING_SIZE:      return "access beyond maximum pinned size";
	case CU_FILE_INVALID_MAPPING_RANGE:     return "access beyond mapped size";
	case CU_FILE_INVALID_FILE_TYPE:         return "unsupported file type";
	case CU_FILE_INVALID_FILE_OPEN_FLAG:    return "unsupported file open flags";
	case CU_FILE_DIO_NOT_SET:               return "fd direct IO not set";
	case CU_FILE_INVALID_VALUE:             return "invalid arguments";
	case CU_FILE_MEMORY_ALREADY_REGISTERED: return "device pointer already registered";
	case CU_FILE_MEMORY_NOT_REGISTERED:     return "device pointer lookup failure";
	case CU_FILE_PERMISSION_DENIED:         return "driver or file access error";
	case CU_FILE_DRIVER_ALREADY_OPEN:       return "driver is already open";
	case CU_FILE_HANDLE_NOT_REGISTERED:     return "file descriptor is not registered";
	case CU_FILE_HANDLE_ALREADY_REGISTERED: return "file descriptor is already registered";
	case CU_FILE_DEVICE_NOT_FOUND:          return "GPU device not found";
	case CU_FILE_INTERNAL_ERROR:            return "internal error";
	case CU_FILE_GETNEWFD_FAILED:           return "failed to obtain new file descriptor";
	case CU_FILE_NVFS_SETUP_ERROR:          return "NVFS driver initialization error";
	case CU_FILE_IO_DISABLED:               return "GPUDirect Storage disabled by config on current file";
	case CU_FILE_BATCH_SUBMIT_FAILED:       return "failed to submit batch operation";
	case CU_FILE_GPU_MEMORY_PINNING_FAILED: return "failed to allocate pinned GPU Memory";
	case CU_FILE_BATCH_FULL:                return "queue full for batch operation";
	case CU_FILE_ASYNC_NOT_SUPPORTED:       return "cuFile stream operation not supported";
	case CU_FILE_IO_MAX_ERROR:              return "GPUDirect Storage Max Error";
	default:                                return "unknown cufile error";
	}
}
/* clang-format on */

typedef struct CUfileError {
	CUfileOpError err;
	CUresult cu_err;
} CUfileError_t;

#define IS_CUFILE_ERR(err) (abs((err)) > CUFILEOP_BASE_ERR)
#define CUFILE_ERRSTR(err) cufileop_status_error((CUfileOpError)abs((err)))
#define IS_CUDA_ERR(status) ((status).err == CU_FILE_CUDA_DRIVER_ERROR)
#define CU_FILE_CUDA_ERR(status) ((status).cu_err)

/* --------------------------------------------------------------------------
 *  Driver properties
 * -------------------------------------------------------------------------- */

typedef enum CUfileDriverStatusFlags {
	CU_FILE_LUSTRE_SUPPORTED = 0,
	CU_FILE_WEKAFS_SUPPORTED = 1,
	CU_FILE_NFS_SUPPORTED = 2,
	CU_FILE_GPFS_SUPPORTED = 3,
	CU_FILE_NVME_SUPPORTED = 4,
	CU_FILE_NVMEOF_SUPPORTED = 5,
	CU_FILE_SCSI_SUPPORTED = 6,
	CU_FILE_SCALEFLUX_CSD_SUPPORTED = 7,
	CU_FILE_NVMESH_SUPPORTED = 8,
	CU_FILE_BEEGFS_SUPPORTED = 9,
	/* 10 reserved for YRCloudFile */
	CU_FILE_NVME_P2P_SUPPORTED = 11,
} CUfileDriverStatusFlags_t;

typedef enum CUfileDriverControlFlags {
	CU_FILE_USE_POLL_MODE = 0,
	CU_FILE_ALLOW_COMPAT_MODE = 1,
} CUfileDriverControlFlags_t;

typedef enum CUfileFeatureFlags {
	CU_FILE_DYN_ROUTING_SUPPORTED = 0,
	CU_FILE_BATCH_IO_SUPPORTED = 1,
	CU_FILE_STREAMS_SUPPORTED = 2,
	CU_FILE_PARALLEL_IO_SUPPORTED = 3,
} CUfileFeatureFlags_t;

typedef struct CUfileDrvProps {
	struct {
		unsigned int major_version;
		unsigned int minor_version;
		size_t poll_thresh_size;
		size_t max_direct_io_size;
		unsigned int dstatusflags;
		unsigned int dcontrolflags;
	} nvfs;
	unsigned int fflags;
	unsigned int max_device_cache_size;
	unsigned int per_buffer_cache_size;
	unsigned int max_device_pinned_mem_size;
	unsigned int max_batch_io_size;
	unsigned int max_batch_io_timeout_msecs;
} CUfileDrvProps_t;

/* --------------------------------------------------------------------------
 *  RDMA
 * -------------------------------------------------------------------------- */

typedef struct sockaddr sockaddr_t;

typedef struct cufileRDMAInfo {
	int version;
	int desc_len;
	const char *desc_str;
} cufileRDMAInfo_t;

#define CU_FILE_RDMA_REGISTER 1
#define CU_FILE_RDMA_RELAXED_ORDERING (1 << 1)

/* --------------------------------------------------------------------------
 *  File handle
 * -------------------------------------------------------------------------- */

typedef struct CUfileFSOps {
	/* NULL means discover using fstat */
	const char *(*fs_type)(void *handle);
	/* list of host addresses to use, NULL means no restriction */
	int (*getRDMADeviceList)(void *handle, sockaddr_t **hostaddrs);
	/* -1 no pref */
	int (*getRDMADevicePriority)(void *handle, char *, size_t, loff_t,
	                             sockaddr_t *hostaddr);
	/* NULL means try VFS */
	ssize_t (*read)(void *handle, char *, size_t, loff_t,
	                cufileRDMAInfo_t *);
	ssize_t (*write)(void *handle, const char *, size_t, loff_t,
	                 cufileRDMAInfo_t *);
} CUfileFSOps_t;

enum CUfileFileHandleType {
	CU_FILE_HANDLE_TYPE_OPAQUE_FD = 1,
	CU_FILE_HANDLE_TYPE_OPAQUE_WIN32 = 2,
	CU_FILE_HANDLE_TYPE_USERSPACE_FS = 3,
};

typedef struct CUfileDescr_t {
	enum CUfileFileHandleType type;
	union {
		int fd;
		void *handle;
	} handle;
	const CUfileFSOps_t *fs_ops;
} CUfileDescr_t;

typedef void *CUfileHandle_t;

/* --------------------------------------------------------------------------
 *  File I/O
 * -------------------------------------------------------------------------- */

CUfileError_t cuFileHandleRegister(CUfileHandle_t *fh, CUfileDescr_t *descr);
void cuFileHandleDeregister(CUfileHandle_t fh);

CUfileError_t cuFileBufRegister(const void *bufPtr_base, size_t length,
                                int flags);
CUfileError_t cuFileBufDeregister(const void *bufPtr_base);

ssize_t cuFileRead(CUfileHandle_t fh, void *bufPtr_base, size_t size,
                   off_t file_offset, off_t bufPtr_offset);
ssize_t cuFileWrite(CUfileHandle_t fh, const void *bufPtr_base, size_t size,
                    off_t file_offset, off_t bufPtr_offset);

/* --------------------------------------------------------------------------
 *  Driver lifecycle
 * -------------------------------------------------------------------------- */

CUfileError_t cuFileDriverOpen(void);

CUfileError_t cuFileDriverClose(void);
#define cuFileDriverClose cuFileDriverClose_v2
CUfileError_t cuFileDriverClose_v2(void);

long cuFileUseCount(void);

CUfileError_t cuFileDriverGetProperties(CUfileDrvProps_t *props);
CUfileError_t cuFileDriverSetPollMode(bool poll, size_t poll_threshold_size);
CUfileError_t cuFileDriverSetMaxDirectIOSize(size_t max_direct_io_size);
CUfileError_t cuFileDriverSetMaxCacheSize(size_t max_cache_size);
CUfileError_t cuFileDriverSetMaxPinnedMemSize(size_t max_pinned_size);

/* --------------------------------------------------------------------------
 *  Batch I/O
 * -------------------------------------------------------------------------- */

typedef enum CUfileOpcode {
	CUFILE_READ = 0,
	CUFILE_WRITE = 1,
} CUfileOpcode_t;

typedef enum CUFILEStatus_enum {
	CUFILE_WAITING = 0x000001,
	CUFILE_PENDING = 0x000002,
	CUFILE_INVALID = 0x000004,
	CUFILE_CANCELED = 0x000008,
	CUFILE_COMPLETE = 0x000010,
	CUFILE_TIMEOUT = 0x000020,
	CUFILE_FAILED = 0x000040,
} CUfileStatus_t;

typedef enum cufileBatchMode {
	CUFILE_BATCH = 1,
} CUfileBatchMode_t;

typedef struct CUfileIOParams {
	CUfileBatchMode_t mode;
	union {
		struct {
			void *devPtr_base;
			off_t file_offset;
			off_t devPtr_offset;
			size_t size;
		} batch;
	} u;
	CUfileHandle_t fh;
	CUfileOpcode_t opcode;
	void *cookie;
} CUfileIOParams_t;

typedef struct CUfileIOEvents {
	void *cookie;
	CUfileStatus_t status;
	size_t ret;
} CUfileIOEvents_t;

typedef void *CUfileBatchHandle_t;

CUfileError_t cuFileBatchIOSetUp(CUfileBatchHandle_t *batch_idp, unsigned nr);
CUfileError_t cuFileBatchIOSubmit(CUfileBatchHandle_t batch_idp, unsigned nr,
                                  CUfileIOParams_t *iocbp, unsigned int flags);
CUfileError_t cuFileBatchIOGetStatus(CUfileBatchHandle_t batch_idp,
                                     unsigned min_nr, unsigned *nr,
                                     CUfileIOEvents_t *iocbp,
                                     struct timespec *timeout);
CUfileError_t cuFileBatchIOCancel(CUfileBatchHandle_t batch_idp);
void cuFileBatchIODestroy(CUfileBatchHandle_t batch_idp);

/* --------------------------------------------------------------------------
 *  Async / stream I/O
 * -------------------------------------------------------------------------- */

#define CU_FILE_STREAM_FIXED_BUF_OFFSET 1
#define CU_FILE_STREAM_FIXED_FILE_OFFSET 2
#define CU_FILE_STREAM_FIXED_FILE_SIZE 4
#define CU_FILE_STREAM_PAGE_ALIGNED_INPUTS 8

CUfileError_t cuFileReadAsync(CUfileHandle_t fh, void *bufPtr_base,
                              size_t *size_p, off_t *file_offset_p,
                              off_t *bufPtr_offset_p, ssize_t *bytes_read_p,
                              CUstream stream);
CUfileError_t cuFileWriteAsync(CUfileHandle_t fh, void *bufPtr_base,
                               size_t *size_p, off_t *file_offset_p,
                               off_t *bufPtr_offset_p, ssize_t *bytes_written_p,
                               CUstream stream);

CUfileError_t cuFileStreamRegister(CUstream stream, unsigned flags);
CUfileError_t cuFileStreamDeregister(CUstream stream);

/* --------------------------------------------------------------------------
 *  Version
 * -------------------------------------------------------------------------- */

CUfileError_t cuFileGetVersion(int *version);

#endif /* __CUFILE_H_ */

#ifdef __cplusplus
}
#endif
