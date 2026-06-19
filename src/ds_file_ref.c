#define _GNU_SOURCE

#include "ds_file_internal.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

struct ref_handle {
	int fd;
};

static bool driver_open;
static long use_count;

#define REF_MAX_BUFS 64
static const void *ref_registered[REF_MAX_BUFS];
static int ref_registered_count;

ds_file_error_t
ds_file_driver_open(void)
{
	if (driver_open)
		return ds_file_err(DS_FILE_DRIVER_ALREADY_OPEN);
	driver_open = true;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_close(void)
{
	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	ref_registered_count = 0;
	driver_open = false;
	return ds_file_ok();
}

long
ds_file_use_count(void)
{
	return use_count;
}

ds_file_error_t
ds_file_driver_get_properties(ds_file_drv_props_t *props)
{
	if (!props)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	memset(props, 0, sizeof(*props));
	props->major_version = 1;
	props->minor_version = 0;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_set_max_direct_io_size(size_t max_direct_io_size)
{
	(void)max_direct_io_size;
	return ds_file_ok();
}

ds_file_error_t
ds_file_get_version(unsigned *major, unsigned *minor, unsigned *patch)
{
	if (major)
		*major = 0;
	if (minor)
		*minor = 1;
	if (patch)
		*patch = 0;
	return ds_file_ok();
}

ds_file_error_t
ds_file_handle_register(ds_file_handle_t *fh, int fd)
{
	if (!fh || fd < 0)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	struct ref_handle *h = malloc(sizeof(*h));
	if (!h)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	h->fd = fd;
	*fh = h;
	use_count++;
	return ds_file_ok();
}

void
ds_file_handle_deregister(ds_file_handle_t fh)
{
	if (!fh)
		return;
	free(fh);
	use_count--;
}

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

void *
ds_file_alloc(size_t size)
{
	return aligned_alloc(4096, ALIGN_UP(size, 4096));
}

void
ds_file_free(void *buf)
{
	if (!buf)
		return;

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf)
			return;
	}
	free(buf);
}

ds_file_error_t
ds_file_buf_register(const void *buf_base, size_t size, int flags)
{
	(void)flags;

	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!buf_base || !size)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf_base)
			return ds_file_err(DS_FILE_MEMORY_ALREADY_REGISTERED);
	}
	if (ref_registered_count >= REF_MAX_BUFS)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	ref_registered[ref_registered_count++] = buf_base;
	return ds_file_ok();
}

ds_file_error_t
ds_file_buf_deregister(const void *buf_base)
{
	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!buf_base)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf_base) {
			ref_registered[i] =
			        ref_registered[--ref_registered_count];
			return ds_file_ok();
		}
	}
	return ds_file_err(DS_FILE_MEMORY_NOT_REGISTERED);
}

ssize_t
ds_file_read(ds_file_handle_t fh, void *buf_base, size_t size,
             off_t file_offset, off_t buf_offset)
{
	struct ref_handle *h = fh;
	ssize_t ret =
	        pread(h->fd, (char *)buf_base + buf_offset, size, file_offset);
	if (ret < 0)
		return -(ssize_t)DS_FILE_INTERNAL_ERROR;
	return ret;
}

ssize_t
ds_file_write(ds_file_handle_t fh, const void *buf_base, size_t size,
              off_t file_offset, off_t buf_offset)
{
	struct ref_handle *h = fh;
	ssize_t ret = pwrite(h->fd, (const char *)buf_base + buf_offset, size,
	                     file_offset);
	if (ret < 0)
		return -(ssize_t)DS_FILE_INTERNAL_ERROR;
	return ret;
}

struct ref_batch {
	ds_file_io_events_t *events;
	unsigned count;
	unsigned capacity;
};

ds_file_error_t
ds_file_batch_io_setup(ds_file_batch_handle_t *batch_idp, unsigned nr)
{
	if (!batch_idp || nr == 0)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	struct ref_batch *b = calloc(1, sizeof(*b));
	if (!b)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	b->events = calloc(nr, sizeof(*b->events));
	if (!b->events) {
		free(b);
		return ds_file_err(DS_FILE_INTERNAL_ERROR);
	}

	b->capacity = nr;
	*batch_idp = b;
	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_submit(ds_file_batch_handle_t batch_idp, unsigned nr,
                        ds_file_io_params_t *iocbp, unsigned int flags)
{
	(void)flags;
	struct ref_batch *b = batch_idp;

	if (!b || !iocbp)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (unsigned i = 0; i < nr; i++) {
		if (b->count >= b->capacity)
			return ds_file_err(DS_FILE_BATCH_FULL);

		ds_file_io_params_t *p = &iocbp[i];
		ssize_t ret;

		if (p->opcode == DS_FILE_READ) {
			ret = ds_file_read(p->fh, p->u.batch.dev_ptr_base,
			                   p->u.batch.size,
			                   p->u.batch.file_offset,
			                   p->u.batch.dev_ptr_offset);
		} else {
			ret = ds_file_write(p->fh, p->u.batch.dev_ptr_base,
			                    p->u.batch.size,
			                    p->u.batch.file_offset,
			                    p->u.batch.dev_ptr_offset);
		}

		ds_file_io_events_t *e = &b->events[b->count++];
		e->cookie = p->cookie;
		if (ret < 0) {
			e->status = DS_FILE_FAILED;
			e->ret = 0;
		} else {
			e->status = DS_FILE_COMPLETE;
			e->ret = (size_t)ret;
		}
	}

	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_get_status(ds_file_batch_handle_t batch_idp, unsigned min_nr,
                            unsigned *nr, ds_file_io_events_t *iocbp,
                            struct timespec *timeout)
{
	(void)timeout;
	struct ref_batch *b = batch_idp;

	if (!b || !nr || !iocbp)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	if (b->count < min_nr)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	unsigned n = b->count < *nr ? b->count : *nr;
	memcpy(iocbp, b->events, n * sizeof(*iocbp));
	*nr = n;
	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_cancel(ds_file_batch_handle_t batch_idp)
{
	struct ref_batch *b = batch_idp;

	if (!b)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (unsigned i = 0; i < b->count; i++)
		b->events[i].status = DS_FILE_CANCELED;

	return ds_file_ok();
}

void
ds_file_batch_io_destroy(ds_file_batch_handle_t batch_idp)
{
	struct ref_batch *b = batch_idp;
	if (!b)
		return;
	free(b->events);
	free(b);
}

ds_file_error_t
ds_file_read_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                   off_t *file_offset_p, off_t *buf_offset_p,
                   ssize_t *bytes_read_p, ds_stream_t stream)
{
	(void)stream;
	ssize_t ret = ds_file_read(fh, buf_base, *size_p, *file_offset_p,
	                           *buf_offset_p);
	*bytes_read_p = ret;
	return ret < 0 ? ds_file_err(DS_FILE_INTERNAL_ERROR) : ds_file_ok();
}

ds_file_error_t
ds_file_write_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                    off_t *file_offset_p, off_t *buf_offset_p,
                    ssize_t *bytes_written_p, ds_stream_t stream)
{
	(void)stream;
	ssize_t ret = ds_file_write(fh, buf_base, *size_p, *file_offset_p,
	                            *buf_offset_p);
	*bytes_written_p = ret;
	return ret < 0 ? ds_file_err(DS_FILE_INTERNAL_ERROR) : ds_file_ok();
}

ds_file_error_t
ds_file_stream_register(ds_stream_t stream, unsigned flags)
{
	(void)stream;
	(void)flags;
	return ds_file_ok();
}

ds_file_error_t
ds_file_stream_deregister(ds_stream_t stream)
{
	(void)stream;
	return ds_file_ok();
}
