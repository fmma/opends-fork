/* SPDX-License-Identifier: BSD-3-Clause */
#define _GNU_SOURCE

#include "opends_internal.h"

#include <sched.h>
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

opends_error_t
opends_driver_open(void)
{
	if (driver_open)
		return opends_err(OPENDS_DRIVER_ALREADY_OPEN);
	driver_open = true;
	return opends_ok();
}

opends_error_t
opends_driver_close(void)
{
	if (!driver_open)
		return opends_err(OPENDS_DRIVER_NOT_INITIALIZED);
	ref_registered_count = 0;
	driver_open = false;
	return opends_ok();
}

long
opends_use_count(void)
{
	return use_count;
}

opends_error_t
opends_driver_get_properties(opends_drv_props_t *props)
{
	if (!props)
		return opends_err(OPENDS_INVALID_VALUE);
	memset(props, 0, sizeof(*props));
	props->major_version = 1;
	props->minor_version = 0;
	return opends_ok();
}

opends_error_t
opends_driver_set_max_direct_io_size(size_t max_direct_io_size)
{
	(void)max_direct_io_size;
	return opends_ok();
}

opends_error_t
opends_get_version(unsigned *major, unsigned *minor, unsigned *patch)
{
	if (major)
		*major = 0;
	if (minor)
		*minor = 1;
	if (patch)
		*patch = 0;
	return opends_ok();
}

opends_error_t
opends_handle_register(opends_handle_t *fh, int fd)
{
	if (!fh || fd < 0)
		return opends_err(OPENDS_INVALID_VALUE);

	struct ref_handle *h = malloc(sizeof(*h));
	if (!h)
		return opends_err(OPENDS_INTERNAL_ERROR);

	h->fd = fd;
	*fh = h;
	use_count++;
	return opends_ok();
}

void
opends_handle_deregister(opends_handle_t fh)
{
	if (!fh)
		return;
	free(fh);
	use_count--;
}

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

void *
opends_alloc(size_t size)
{
	return aligned_alloc(4096, ALIGN_UP(size, 4096));
}

void
opends_free(void *buf)
{
	if (!buf)
		return;

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf)
			return;
	}
	free(buf);
}

opends_error_t
opends_buf_register(const void *buf_base, size_t size, int flags)
{
	(void)flags;

	if (!driver_open)
		return opends_err(OPENDS_DRIVER_NOT_INITIALIZED);
	if (!buf_base || !size)
		return opends_err(OPENDS_INVALID_VALUE);

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf_base)
			return opends_err(OPENDS_MEMORY_ALREADY_REGISTERED);
	}
	if (ref_registered_count >= REF_MAX_BUFS)
		return opends_err(OPENDS_INTERNAL_ERROR);

	ref_registered[ref_registered_count++] = buf_base;
	return opends_ok();
}

opends_error_t
opends_buf_deregister(const void *buf_base)
{
	if (!driver_open)
		return opends_err(OPENDS_DRIVER_NOT_INITIALIZED);
	if (!buf_base)
		return opends_err(OPENDS_INVALID_VALUE);

	for (int i = 0; i < ref_registered_count; i++) {
		if (ref_registered[i] == buf_base) {
			ref_registered[i] =
			        ref_registered[--ref_registered_count];
			return opends_ok();
		}
	}
	return opends_err(OPENDS_MEMORY_NOT_REGISTERED);
}

static opends_error_t
ref_async_submit(bool is_write, opends_handle_t fh, void *buf_base, size_t size,
                 off_t file_offset, off_t buf_offset,
                 opends_async_future_t *future)
{
	if (!driver_open)
		return opends_err(OPENDS_DRIVER_NOT_INITIALIZED);
	if (!fh || !buf_base || !future)
		return opends_err(OPENDS_INVALID_VALUE);

	ssize_t result;
	if (is_write)
		result = opends_sync_write(fh, buf_base, size, file_offset,
		                           buf_offset);
	else
		result = opends_sync_read(fh, buf_base, size, file_offset,
		                          buf_offset);

	future->result = result;
	__atomic_store_n(&future->done, 1, __ATOMIC_RELEASE);
	return opends_ok();
}

opends_error_t
opends_async_read(opends_handle_t fh, void *buf_base, size_t size,
                  off_t file_offset, off_t buf_offset,
                  opends_async_future_t *future)
{
	return ref_async_submit(false, fh, buf_base, size, file_offset,
	                        buf_offset, future);
}

opends_error_t
opends_async_write(opends_handle_t fh, const void *buf_base, size_t size,
                   off_t file_offset, off_t buf_offset,
                   opends_async_future_t *future)
{
	return ref_async_submit(true, fh, (void *)buf_base, size, file_offset,
	                        buf_offset, future);
}

ssize_t
opends_async_await(opends_async_future_t *future)
{
	if (!future)
		return -(ssize_t)OPENDS_INVALID_VALUE;

	while (!__atomic_load_n(&future->done, __ATOMIC_ACQUIRE))
		sched_yield();

	return future->result;
}

ssize_t
opends_sync_read(opends_handle_t fh, void *buf_base, size_t size,
                 off_t file_offset, off_t buf_offset)
{
	struct ref_handle *h = fh;
	ssize_t ret =
	        pread(h->fd, (char *)buf_base + buf_offset, size, file_offset);
	if (ret < 0)
		return -(ssize_t)OPENDS_INTERNAL_ERROR;
	return ret;
}

ssize_t
opends_sync_write(opends_handle_t fh, const void *buf_base, size_t size,
                  off_t file_offset, off_t buf_offset)
{
	struct ref_handle *h = fh;
	ssize_t ret = pwrite(h->fd, (const char *)buf_base + buf_offset, size,
	                     file_offset);
	if (ret < 0)
		return -(ssize_t)OPENDS_INTERNAL_ERROR;
	return ret;
}

opends_error_t
opends_stream_read(opends_handle_t fh, void *buf_base, size_t *size_p,
                   off_t *file_offset_p, off_t *buf_offset_p,
                   ssize_t *bytes_read_p, opends_stream_t stream)
{
	(void)stream;
	ssize_t ret = opends_sync_read(fh, buf_base, *size_p, *file_offset_p,
	                               *buf_offset_p);
	*bytes_read_p = ret;
	return ret < 0 ? opends_err(OPENDS_INTERNAL_ERROR) : opends_ok();
}

opends_error_t
opends_stream_write(opends_handle_t fh, void *buf_base, size_t *size_p,
                    off_t *file_offset_p, off_t *buf_offset_p,
                    ssize_t *bytes_written_p, opends_stream_t stream)
{
	(void)stream;
	ssize_t ret = opends_sync_write(fh, buf_base, *size_p, *file_offset_p,
	                                *buf_offset_p);
	*bytes_written_p = ret;
	return ret < 0 ? opends_err(OPENDS_INTERNAL_ERROR) : opends_ok();
}

opends_error_t
opends_stream_register(opends_stream_t stream, unsigned flags)
{
	(void)stream;
	(void)flags;
	return opends_ok();
}

opends_error_t
opends_stream_deregister(opends_stream_t stream)
{
	(void)stream;
	return opends_ok();
}

struct ref_batch {
	opends_io_events_t *events;
	unsigned count;
	unsigned capacity;
};

opends_error_t
opends_batch_setup(opends_batch_handle_t *batch_idp, unsigned nr)
{
	if (!batch_idp || nr == 0)
		return opends_err(OPENDS_INVALID_VALUE);

	struct ref_batch *b = calloc(1, sizeof(*b));
	if (!b)
		return opends_err(OPENDS_INTERNAL_ERROR);

	b->events = calloc(nr, sizeof(*b->events));
	if (!b->events) {
		free(b);
		return opends_err(OPENDS_INTERNAL_ERROR);
	}

	b->capacity = nr;
	*batch_idp = b;
	return opends_ok();
}

opends_error_t
opends_batch_submit(opends_batch_handle_t batch_idp, unsigned nr,
                    opends_io_params_t *iocbp, unsigned int flags)
{
	(void)flags;
	struct ref_batch *b = batch_idp;

	if (!b || !iocbp)
		return opends_err(OPENDS_INVALID_VALUE);

	for (unsigned i = 0; i < nr; i++) {
		if (b->count >= b->capacity)
			return opends_err(OPENDS_BATCH_FULL);

		opends_io_params_t *p = &iocbp[i];
		ssize_t ret;

		if (p->opcode == OPENDS_READ) {
			ret = opends_sync_read(p->fh, p->u.batch.dev_ptr_base,
			                       p->u.batch.size,
			                       p->u.batch.file_offset,
			                       p->u.batch.dev_ptr_offset);
		} else {
			ret = opends_sync_write(p->fh, p->u.batch.dev_ptr_base,
			                        p->u.batch.size,
			                        p->u.batch.file_offset,
			                        p->u.batch.dev_ptr_offset);
		}

		opends_io_events_t *e = &b->events[b->count++];
		e->cookie = p->cookie;
		if (ret < 0) {
			e->status = OPENDS_FAILED;
			e->ret = 0;
		} else {
			e->status = OPENDS_COMPLETE;
			e->ret = (size_t)ret;
		}
	}

	return opends_ok();
}

opends_error_t
opends_batch_get_status(opends_batch_handle_t batch_idp, unsigned min_nr,
                        unsigned *nr, opends_io_events_t *iocbp,
                        struct timespec *timeout)
{
	(void)timeout;
	struct ref_batch *b = batch_idp;

	if (!b || !nr || !iocbp)
		return opends_err(OPENDS_INVALID_VALUE);
	if (b->count < min_nr)
		return opends_err(OPENDS_INVALID_VALUE);

	unsigned n = b->count < *nr ? b->count : *nr;
	memcpy(iocbp, b->events, n * sizeof(*iocbp));
	*nr = n;
	return opends_ok();
}

opends_error_t
opends_batch_cancel(opends_batch_handle_t batch_idp)
{
	struct ref_batch *b = batch_idp;

	if (!b)
		return opends_err(OPENDS_INVALID_VALUE);

	for (unsigned i = 0; i < b->count; i++)
		b->events[i].status = OPENDS_CANCELED;

	return opends_ok();
}

void
opends_batch_destroy(opends_batch_handle_t batch_idp)
{
	struct ref_batch *b = batch_idp;
	if (!b)
		return;
	free(b->events);
	free(b);
}
