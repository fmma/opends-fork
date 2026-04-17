/*
 * ds_file_aisio.c - aisio backend for raw-NVMe direct storage.
 *
 * Reads go straight from an NVMe device into GPU memory via xNVMe's
 * upcie-cuda backend (PCIe P2P DMA, no filesystem in the path). File
 * offsets are translated to physical LBAs through a HOMI mock client
 * that reads a pre-built extent cache (see tools/cache_extents).
 *
 * Requires: libxnvme and the CUDA toolkit. The NVMe kernel driver must
 * be unbound from the target device before ds_file_driver_open runs.
 *
 * Write, batch, and async paths report DS_FILE_IO_NOT_SUPPORTED and
 * DS_FILE_ASYNC_NOT_SUPPORTED; only synchronous reads are implemented.
 */

#define _GNU_SOURCE

#include "ds_file_internal.h"
#include "homi_client_mock.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libxnvme.h>

#define HOMI_SHM_NAME "/homi"
#define MAX_BUF_ENTRIES 64
#define DEFAULT_BOUNCE_SIZE (128 * 1024)
#define NVME_MAX_NLB 65536

struct buf_entry {
	const void *base;
	size_t length;
};

struct aisio_handle {
	int fd;
};

struct aisio_driver {
	struct homi_conn *homi;
	struct xnvme_dev *xdev;
	uint32_t nsid;
	uint32_t lba_size;
	uint32_t mdts_nbytes;
	struct buf_entry bufs[MAX_BUF_ENTRIES];
	int buf_count;
};

static struct aisio_driver *drv;
static long use_count;

/* xNVMe ships XNVME_MIN_U64 but no matching max; define a local one. */
static uint64_t
max_u64(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

static int
open_device(struct aisio_driver *d, int fd)
{
	char *uri = NULL;
	int rc = homi_get_device_uri(d->homi, fd, &uri);
	if (rc < 0)
		return rc;

	struct xnvme_opts opts = xnvme_opts_default();
	opts.be = "upcie-cuda";

	d->xdev = xnvme_dev_open(uri, &opts);
	free(uri);
	if (!d->xdev)
		return -EIO;

	const struct xnvme_geo *geo = xnvme_dev_get_geo(d->xdev);
	d->nsid = xnvme_dev_get_nsid(d->xdev);
	d->lba_size = geo->lba_nbytes ? geo->lba_nbytes : geo->nbytes;
	d->mdts_nbytes =
	        geo->mdts_nbytes ? geo->mdts_nbytes : DEFAULT_BOUNCE_SIZE;

	return 0;
}

static ssize_t
aisio_read_extents(struct aisio_driver *d, struct aisio_handle *h, void *dst,
                   size_t size, off_t file_offset)
{
	uint32_t lba_nbytes = d->lba_size;
	if (lba_nbytes == 0)
		return -EIO;

	uint64_t req_start = (uint64_t)file_offset;
	if (size > UINT64_MAX - req_start)
		return -EINVAL;
	uint64_t req_end = req_start + size;

	struct homi_extent_list *elist = NULL;
	int rc = homi_get_extents(d->homi, h->fd, &elist);
	if (rc < 0)
		return rc;

	size_t total_transferred = 0;
	struct xnvme_cmd_ctx cmd = xnvme_cmd_ctx_from_dev(d->xdev);
	uint64_t max_chunk_lbas = d->mdts_nbytes / lba_nbytes;
	if (max_chunk_lbas == 0) {
		homi_extent_list_free(elist);
		return -EINVAL;
	}

	for (uint32_t i = 0; i < elist->count; i++) {
		struct homi_extent *e = &elist->extents[i];

		uint64_t ext_start = e->file_offset;
		uint64_t ext_end = ext_start + e->length;

		/* Extents are sorted by file_offset; once we're past the
		 * request no later extent can overlap. */
		if (ext_start >= req_end)
			break;

		uint64_t span_start = max_u64(req_start, ext_start);
		uint64_t span_end = XNVME_MIN_U64(req_end, ext_end);
		if (span_start >= span_end)
			continue;

		uint64_t off_in_ext = span_start - ext_start;
		size_t buf_off = span_start - req_start;

		/* TODO: bounce buffer for unaligned requests. */
		if (off_in_ext % lba_nbytes != 0 ||
		    (span_end - span_start) % lba_nbytes != 0) {
			homi_extent_list_free(elist);
			return -EINVAL;
		}

		uint64_t first_lba_off = off_in_ext / lba_nbytes;
		uint64_t nlbas_total = (span_end - span_start) / lba_nbytes;
		uint64_t slba = e->slba + first_lba_off;

		uint64_t lbas_done = 0;
		while (lbas_done < nlbas_total) {
			uint64_t chunk_lbas = XNVME_MIN_U64(
			        nlbas_total - lbas_done, max_chunk_lbas);
			chunk_lbas = XNVME_MIN_U64(chunk_lbas, NVME_MAX_NLB);

			uint16_t nlb = (uint16_t)(chunk_lbas - 1);
			uint64_t cur_slba = slba + lbas_done;
			uint8_t *dst_chunk = (uint8_t *)dst + buf_off +
			                     lbas_done * lba_nbytes;

			rc = xnvme_nvm_read(&cmd, d->nsid, cur_slba, nlb,
			                    dst_chunk, NULL);
			if (rc || xnvme_cmd_ctx_cpl_status(&cmd)) {
				homi_extent_list_free(elist);
				return -EIO;
			}

			lbas_done += chunk_lbas;
			total_transferred += chunk_lbas * lba_nbytes;
		}
	}

	homi_extent_list_free(elist);
	return (ssize_t)total_transferred;
}

/* ------------------------------------------------------------------ */
/*  Driver lifecycle                                                   */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_driver_open(void)
{
	if (drv)
		return ds_file_err(DS_FILE_DRIVER_ALREADY_OPEN);

	struct aisio_driver *d = calloc(1, sizeof(*d));
	if (!d)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	int rc = homi_connect(HOMI_SHM_NAME, &d->homi);
	if (rc < 0) {
		free(d);
		return ds_file_err(DS_FILE_FS_SETUP_ERROR);
	}

	drv = d;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_close(void)
{
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);

	for (int i = 0; i < drv->buf_count; i++)
		xnvme_buf_free(drv->xdev, (void *)drv->bufs[i].base);
	drv->buf_count = 0;

	if (drv->xdev)
		xnvme_dev_close(drv->xdev);
	if (drv->homi)
		homi_disconnect(drv->homi);

	free(drv);
	drv = NULL;
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
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!props)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	memset(props, 0, sizeof(*props));
	props->major_version = 0;
	props->minor_version = 1;
	props->max_direct_io_size = drv->mdts_nbytes;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_set_max_direct_io_size(size_t max_direct_io_size)
{
	(void)max_direct_io_size;
	return drv ? ds_file_ok() : ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
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

/* ------------------------------------------------------------------ */
/*  Handle registration                                                */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_handle_register(ds_file_handle_t *fh, int fd)
{
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!fh)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	if (!drv->xdev) {
		int rc = open_device(drv, fd);
		if (rc < 0)
			return ds_file_err(DS_FILE_DEVICE_NOT_FOUND);
	}

	struct aisio_handle *h = calloc(1, sizeof(*h));
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

/* ------------------------------------------------------------------ */
/*  Buffer allocation                                                  */
/* ------------------------------------------------------------------ */

void *
ds_file_alloc(size_t size)
{
	if (!drv || !drv->xdev || drv->buf_count >= MAX_BUF_ENTRIES)
		return NULL;

	void *buf = xnvme_buf_alloc(drv->xdev, size);
	if (!buf)
		return NULL;

	struct buf_entry *e = &drv->bufs[drv->buf_count++];
	e->base = buf;
	e->length = size;
	return buf;
}

void
ds_file_free(void *buf)
{
	if (!drv || !buf)
		return;

	for (int i = 0; i < drv->buf_count; i++) {
		if (drv->bufs[i].base == buf) {
			drv->bufs[i] = drv->bufs[drv->buf_count - 1];
			drv->buf_count--;
			xnvme_buf_free(drv->xdev, buf);
			return;
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Synchronous I/O                                                    */
/* ------------------------------------------------------------------ */

ssize_t
ds_file_read(ds_file_handle_t fh, void *buf_base, size_t size,
             off_t file_offset, off_t buf_offset)
{
	if (!drv)
		return -(ssize_t)DS_FILE_DRIVER_NOT_INITIALIZED;
	if (!fh || !buf_base)
		return -(ssize_t)DS_FILE_INVALID_VALUE;

	void *dst = (uint8_t *)buf_base + buf_offset;
	ssize_t n = aisio_read_extents(drv, (struct aisio_handle *)fh, dst,
	                               size, file_offset);
	if (n < 0)
		return -(ssize_t)DS_FILE_DEVICE_DRIVER_ERROR;

	return n;
}

ssize_t
ds_file_write(ds_file_handle_t fh, const void *buf_base, size_t size,
              off_t file_offset, off_t buf_offset)
{
	(void)fh;
	(void)buf_base;
	(void)size;
	(void)file_offset;
	(void)buf_offset;
	return -(ssize_t)DS_FILE_IO_NOT_SUPPORTED;
}

/* ------------------------------------------------------------------ */
/*  Batch I/O (not implemented)                                        */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_batch_io_setup(ds_file_batch_handle_t *batch_idp, unsigned nr)
{
	(void)batch_idp;
	(void)nr;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_batch_io_submit(ds_file_batch_handle_t batch_idp, unsigned nr,
                        ds_file_io_params_t *iocbp, unsigned int flags)
{
	(void)batch_idp;
	(void)nr;
	(void)iocbp;
	(void)flags;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_batch_io_get_status(ds_file_batch_handle_t batch_idp, unsigned min_nr,
                            unsigned *nr, ds_file_io_events_t *iocbp,
                            struct timespec *timeout)
{
	(void)batch_idp;
	(void)min_nr;
	(void)nr;
	(void)iocbp;
	(void)timeout;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_batch_io_cancel(ds_file_batch_handle_t batch_idp)
{
	(void)batch_idp;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

void
ds_file_batch_io_destroy(ds_file_batch_handle_t batch_idp)
{
	(void)batch_idp;
}

/* ------------------------------------------------------------------ */
/*  Async (stream) I/O (not implemented)                               */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_read_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                   off_t *file_offset_p, off_t *buf_offset_p,
                   ssize_t *bytes_read_p, ds_stream_t stream)
{
	(void)fh;
	(void)buf_base;
	(void)size_p;
	(void)file_offset_p;
	(void)buf_offset_p;
	(void)bytes_read_p;
	(void)stream;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_write_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                    off_t *file_offset_p, off_t *buf_offset_p,
                    ssize_t *bytes_written_p, ds_stream_t stream)
{
	(void)fh;
	(void)buf_base;
	(void)size_p;
	(void)file_offset_p;
	(void)buf_offset_p;
	(void)bytes_written_p;
	(void)stream;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_stream_register(ds_stream_t stream, unsigned flags)
{
	(void)stream;
	(void)flags;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}

ds_file_error_t
ds_file_stream_deregister(ds_stream_t stream)
{
	(void)stream;
	return ds_file_err(DS_FILE_ASYNC_NOT_SUPPORTED);
}
