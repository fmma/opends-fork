/*
 * ds_file_batch.h - Batch I/O interface for ds_file.
 *
 * Allows submitting multiple I/O operations in a single call and
 * polling for completion. Useful for overlapping I/O with computation.
 *
 * The reference backend executes operations synchronously on submit
 * and marks them complete immediately.
 */
#ifndef DS_FILE_BATCH_H_
#define DS_FILE_BATCH_H_

#include "ds_file.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ds_file_opcode {
	DS_FILE_READ = 0,
	DS_FILE_WRITE = 1,
} ds_file_opcode_t;

typedef enum ds_file_status {
	DS_FILE_WAITING = 0x000001,
	DS_FILE_PENDING = 0x000002,
	DS_FILE_INVALID = 0x000004,
	DS_FILE_CANCELED = 0x000008,
	DS_FILE_COMPLETE = 0x000010,
	DS_FILE_TIMEOUT = 0x000020,
	DS_FILE_FAILED = 0x000040,
} ds_file_status_t;

typedef enum ds_file_batch_mode {
	DS_FILE_BATCH = 1,
} ds_file_batch_mode_t;

typedef struct ds_file_io_params {
	ds_file_batch_mode_t mode;
	union {
		struct {
			void *dev_ptr_base;
			off_t file_offset;
			off_t dev_ptr_offset;
			size_t size;
		} batch;
	} u;
	ds_file_handle_t fh;
	ds_file_opcode_t opcode;
	void *cookie;
} ds_file_io_params_t;

typedef struct ds_file_io_events {
	void *cookie;
	ds_file_status_t status;
	size_t ret;
} ds_file_io_events_t;

typedef void *ds_file_batch_handle_t;

/* Create a batch context that can hold up to nr outstanding operations. */
ds_file_error_t ds_file_batch_io_setup(ds_file_batch_handle_t *batch_idp,
                                       unsigned nr);
ds_file_error_t ds_file_batch_io_submit(ds_file_batch_handle_t batch_idp,
                                        unsigned nr, ds_file_io_params_t *iocbp,
                                        unsigned int flags);
/* Poll for at least min_nr completions, returning up to *nr events. */
ds_file_error_t ds_file_batch_io_get_status(ds_file_batch_handle_t batch_idp,
                                            unsigned min_nr, unsigned *nr,
                                            ds_file_io_events_t *iocbp,
                                            struct timespec *timeout);
ds_file_error_t ds_file_batch_io_cancel(ds_file_batch_handle_t batch_idp);
void ds_file_batch_io_destroy(ds_file_batch_handle_t batch_idp);

#ifdef __cplusplus
}
#endif

#endif /* DS_FILE_BATCH_H_ */
