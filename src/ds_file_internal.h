/*
 * ds_file_internal.h - Helpers for ds_file backend implementations.
 * Not part of the public API.
 */
#ifndef DS_FILE_INTERNAL_H_
#define DS_FILE_INTERNAL_H_

#include "opengds.h"

static inline ds_file_error_t
ds_file_ok(void)
{
	return (ds_file_error_t){DS_FILE_SUCCESS, 0};
}

static inline ds_file_error_t
ds_file_err(ds_file_op_error_t e)
{
	return (ds_file_error_t){e, 0};
}

#endif /* DS_FILE_INTERNAL_H_ */
