/*
 * ds_file_async.h - Stream-based async I/O interface for ds_file.
 *
 * Operations are associated with a stream (e.g. a CUDA stream) and
 * complete asynchronously. The size, offset, and byte count parameters
 * are pointers so the values can be read at stream execution time
 * rather than submission time.
 *
 * The reference backend executes operations synchronously and reads
 * the pointer parameters immediately.
 */
#ifndef DS_FILE_ASYNC_H_
#define DS_FILE_ASYNC_H_

#include "ds_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *ds_stream_t;

ds_file_error_t ds_file_read_async(ds_file_handle_t fh, void *buf_base,
                                   size_t *size_p, off_t *file_offset_p,
                                   off_t *buf_offset_p, ssize_t *bytes_read_p,
                                   ds_stream_t stream);
ds_file_error_t ds_file_write_async(ds_file_handle_t fh, void *buf_base,
                                    size_t *size_p, off_t *file_offset_p,
                                    off_t *buf_offset_p,
                                    ssize_t *bytes_written_p,
                                    ds_stream_t stream);
ds_file_error_t ds_file_stream_register(ds_stream_t stream, unsigned flags);
ds_file_error_t ds_file_stream_deregister(ds_stream_t stream);

#ifdef __cplusplus
}
#endif

#endif /* DS_FILE_ASYNC_H_ */
