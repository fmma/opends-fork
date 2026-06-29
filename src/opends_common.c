/* SPDX-License-Identifier: BSD-3-Clause */
#include "opends_internal.h"

/* clang-format off */
const char *
opends_op_status_error(opends_op_error_t status)
{
	switch (status) {
	case OPENDS_SUCCESS:                   return "opends success";
	case OPENDS_DRIVER_NOT_INITIALIZED:    return "driver is not loaded";
	case OPENDS_DRIVER_INVALID_PROPS:      return "invalid property";
	case OPENDS_DRIVER_UNSUPPORTED_LIMIT:  return "property range error";
	case OPENDS_DRIVER_VERSION_MISMATCH:   return "driver version mismatch";
	case OPENDS_DRIVER_VERSION_READ_ERROR: return "driver version read error";
	case OPENDS_DRIVER_CLOSING:            return "driver shutdown in progress";
	case OPENDS_PLATFORM_NOT_SUPPORTED:    return "direct storage not supported on current platform";
	case OPENDS_IO_NOT_SUPPORTED:          return "direct storage not supported on current file";
	case OPENDS_DEVICE_NOT_SUPPORTED:      return "direct storage not supported on current device";
	case OPENDS_FS_DRIVER_ERROR:           return "filesystem driver ioctl error";
	case OPENDS_DEVICE_DRIVER_ERROR:       return "device driver API error";
	case OPENDS_POINTER_INVALID:           return "invalid device pointer";
	case OPENDS_MEMORY_TYPE_INVALID:       return "invalid pointer memory type";
	case OPENDS_POINTER_RANGE_ERROR:       return "pointer range exceeds allocated address range";
	case OPENDS_CONTEXT_MISMATCH:          return "device context mismatch";
	case OPENDS_INVALID_MAPPING_SIZE:      return "access beyond maximum pinned size";
	case OPENDS_INVALID_MAPPING_RANGE:     return "access beyond mapped size";
	case OPENDS_INVALID_FILE_TYPE:         return "unsupported file type";
	case OPENDS_INVALID_FILE_OPEN_FLAG:    return "unsupported file open flags";
	case OPENDS_DIO_NOT_SET:               return "fd direct IO not set";
	case OPENDS_INVALID_VALUE:             return "invalid arguments";
	case OPENDS_MEMORY_ALREADY_REGISTERED: return "device pointer already registered";
	case OPENDS_MEMORY_NOT_REGISTERED:     return "device pointer lookup failure";
	case OPENDS_PERMISSION_DENIED:         return "driver or file access error";
	case OPENDS_DRIVER_ALREADY_OPEN:       return "driver is already open";
	case OPENDS_HANDLE_NOT_REGISTERED:     return "file descriptor is not registered";
	case OPENDS_HANDLE_ALREADY_REGISTERED: return "file descriptor is already registered";
	case OPENDS_DEVICE_NOT_FOUND:          return "device not found";
	case OPENDS_INTERNAL_ERROR:            return "internal error";
	case OPENDS_GETNEWFD_FAILED:           return "failed to obtain new file descriptor";
	case OPENDS_FS_SETUP_ERROR:            return "filesystem driver initialization error";
	case OPENDS_IO_DISABLED:               return "direct storage disabled by config on current file";
	case OPENDS_BATCH_SUBMIT_FAILED:       return "failed to submit batch operation";
	case OPENDS_MEMORY_PINNING_FAILED:     return "failed to allocate pinned device memory";
	case OPENDS_BATCH_FULL:                return "queue full for batch operation";
	case OPENDS_ASYNC_NOT_SUPPORTED:       return "stream operation not supported";
	case OPENDS_IO_MAX_ERROR:              return "max error";
	default:                                return "unknown opends error";
	}
}
/* clang-format on */
