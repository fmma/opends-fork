#include "ds_file_internal.h"

/* clang-format off */
const char *
ds_file_op_status_error(ds_file_op_error_t status)
{
	switch (status) {
	case DS_FILE_SUCCESS:                   return "ds_file success";
	case DS_FILE_DRIVER_NOT_INITIALIZED:    return "driver is not loaded";
	case DS_FILE_DRIVER_INVALID_PROPS:      return "invalid property";
	case DS_FILE_DRIVER_UNSUPPORTED_LIMIT:  return "property range error";
	case DS_FILE_DRIVER_VERSION_MISMATCH:   return "driver version mismatch";
	case DS_FILE_DRIVER_VERSION_READ_ERROR: return "driver version read error";
	case DS_FILE_DRIVER_CLOSING:            return "driver shutdown in progress";
	case DS_FILE_PLATFORM_NOT_SUPPORTED:    return "direct storage not supported on current platform";
	case DS_FILE_IO_NOT_SUPPORTED:          return "direct storage not supported on current file";
	case DS_FILE_DEVICE_NOT_SUPPORTED:      return "direct storage not supported on current device";
	case DS_FILE_FS_DRIVER_ERROR:           return "filesystem driver ioctl error";
	case DS_FILE_DEVICE_DRIVER_ERROR:       return "device driver API error";
	case DS_FILE_POINTER_INVALID:           return "invalid device pointer";
	case DS_FILE_MEMORY_TYPE_INVALID:       return "invalid pointer memory type";
	case DS_FILE_POINTER_RANGE_ERROR:       return "pointer range exceeds allocated address range";
	case DS_FILE_CONTEXT_MISMATCH:          return "device context mismatch";
	case DS_FILE_INVALID_MAPPING_SIZE:      return "access beyond maximum pinned size";
	case DS_FILE_INVALID_MAPPING_RANGE:     return "access beyond mapped size";
	case DS_FILE_INVALID_FILE_TYPE:         return "unsupported file type";
	case DS_FILE_INVALID_FILE_OPEN_FLAG:    return "unsupported file open flags";
	case DS_FILE_DIO_NOT_SET:               return "fd direct IO not set";
	case DS_FILE_INVALID_VALUE:             return "invalid arguments";
	case DS_FILE_MEMORY_ALREADY_REGISTERED: return "device pointer already registered";
	case DS_FILE_MEMORY_NOT_REGISTERED:     return "device pointer lookup failure";
	case DS_FILE_PERMISSION_DENIED:         return "driver or file access error";
	case DS_FILE_DRIVER_ALREADY_OPEN:       return "driver is already open";
	case DS_FILE_HANDLE_NOT_REGISTERED:     return "file descriptor is not registered";
	case DS_FILE_HANDLE_ALREADY_REGISTERED: return "file descriptor is already registered";
	case DS_FILE_DEVICE_NOT_FOUND:          return "device not found";
	case DS_FILE_INTERNAL_ERROR:            return "internal error";
	case DS_FILE_GETNEWFD_FAILED:           return "failed to obtain new file descriptor";
	case DS_FILE_FS_SETUP_ERROR:            return "filesystem driver initialization error";
	case DS_FILE_FS_DIRTY:                  return "dirty file system";
	case DS_FILE_IO_DISABLED:               return "direct storage disabled by config on current file";
	case DS_FILE_BATCH_SUBMIT_FAILED:       return "failed to submit batch operation";
	case DS_FILE_MEMORY_PINNING_FAILED:     return "failed to allocate pinned device memory";
	case DS_FILE_BATCH_FULL:                return "queue full for batch operation";
	case DS_FILE_ASYNC_NOT_SUPPORTED:       return "stream operation not supported";
	case DS_FILE_IO_MAX_ERROR:              return "max error";
	default:                                return "unknown ds_file error";
	}
}
/* clang-format on */
