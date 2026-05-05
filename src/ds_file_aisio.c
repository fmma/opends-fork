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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cuda_runtime.h>
#include <libxnvme.h>

#define HOMI_SHM_NAME "/homi"
/* filperf with --batch-size N pre-registers N buffers up front (one
 * per concurrent in-flight request). 8192 covers the imagenetish bench
 * (--batch-size 4096) with margin; it's a flat array of small entries,
 * so headroom is cheap. */
#define MAX_BUF_ENTRIES 8192
#define DEFAULT_BOUNCE_SIZE (128 * 1024)
#define NVME_MAX_NLB 65536
/* NVMe PRP entries on this controller (Samsung 990 PRO) require a
 * page-aligned offset within the first PRP; the device rejects
 * non-aligned PRP1 with status code 0x13 ("PRP Offset Invalid"). The
 * NVMe MPSMIN page is 4 KiB in practice. */
#define NVME_PRP_PAGE 4096

struct buf_entry {
	const void *base;
	size_t length;
	bool owned; /* true: from ds_file_alloc (xnvme_buf_alloc);
	             * false: registered via ds_file_buf_register
	             * (xnvme_mem_map). */
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
	/* NVME_PRP_PAGE-sized scratch used to bounce two flavours of
	 * misaligned reads:
	 *   1. Trailing partial-LBA tail (file size not LBA-multiple).
	 *   2. Leading bytes when the destination buffer is not
	 *      NVME_PRP_PAGE-aligned (cudaMalloc only guarantees 256 B).
	 * Lazy-allocated on first need. Under upcie-cuda it lives on the
	 * GPU (cudamem_dma_malloc); cudaMemcpyDefault routes correctly via
	 * UVA regardless of dst residency. */
	void *bounce_buf;
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
ensure_bounce_buf(struct aisio_driver *d)
{
	if (d->bounce_buf)
		return 0;
	d->bounce_buf = xnvme_buf_alloc(d->xdev, NVME_PRP_PAGE);
	return d->bounce_buf ? 0 : -ENOMEM;
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

	/* xnvme_dev_open can return success with a zeroed geometry if the
	 * controller wasn't fully identified (e.g. opened directly after
	 * the kernel nvme driver shut it down without a PCI reset). Fail
	 * loudly here instead of letting every subsequent read return
	 * -EIO with no context. */
	if (d->lba_size == 0) {
		fprintf(stderr,
		        "aisio open_device: zero geometry from xnvme_dev_open "
		        "(lba_nbytes=%u nbytes=%zu mdts_nbytes=%zu) — "
		        "controller likely needs a PCI reset before this open\n",
		        geo->lba_nbytes, geo->nbytes, geo->mdts_nbytes);
		xnvme_dev_close(d->xdev);
		d->xdev = NULL;
		return -EIO;
	}

	return 0;
}

static ssize_t
aisio_read_extents(struct aisio_driver *d, struct aisio_handle *h, void *dst,
                   size_t size, off_t file_offset)
{
	uint32_t lba_nbytes = d->lba_size;

	uint64_t req_start = (uint64_t)file_offset;
	if (size > UINT64_MAX - req_start)
		return -EINVAL;
	uint64_t req_end = req_start + size;

	const struct homi_extent *extents = NULL;
	uint32_t extent_count = 0;
	int rc = homi_get_extents(d->homi, h->fd, &extents, &extent_count);
	if (rc < 0) {
		fprintf(stderr, "aisio prelude: homi_get_extents fd=%d rc=%d\n", h->fd, rc);
		return rc;
	}

	size_t total_transferred = 0;
	struct xnvme_cmd_ctx cmd = xnvme_cmd_ctx_from_dev(d->xdev);
	uint64_t max_chunk_lbas = d->mdts_nbytes / lba_nbytes;
	if (max_chunk_lbas == 0) {
		fprintf(stderr, "aisio prelude: max_chunk_lbas=0 (mdts=%zu lba=%u)\n",
		        d->mdts_nbytes, lba_nbytes);
		return -EINVAL;
	}

	for (uint32_t i = 0; i < extent_count; i++) {
		const struct homi_extent *e = &extents[i];

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

		/* File-side head-unaligned spans aren't reachable from the
		 * current read paths (file_offset starts at 0 and FS
		 * extents land on FS-block boundaries, which are multiples
		 * of the LBA size). Reject explicitly until a use case
		 * appears. */
		if (off_in_ext % lba_nbytes != 0)
			return -EINVAL;

		uint8_t *abs_dst = (uint8_t *)dst + buf_off;
		size_t remaining = span_end - span_start;
		uint64_t cur_slba = e->slba + (off_in_ext / lba_nbytes);

		/* xnvme reads are LBA-granular, so the caller's buffer
		 * must be LBA-aligned for the partial-byte copies below to
		 * land at the right offsets. cudaMalloc on this driver
		 * happens to return >=512 B alignment, so this is a
		 * defensive check; anything finer needs a real bounce of
		 * arbitrary spans. */
		if ((uintptr_t)abs_dst % lba_nbytes != 0)
			return -EINVAL;

		/* HEAD bounce: NVMe PRP1 must be page-aligned on this
		 * controller. cudaMalloc only guarantees 256-byte
		 * alignment, so for fil's small per-buffer allocations
		 * (e.g. imagenetish ~152 KiB) the destination can land
		 * mid-page. Bounce just enough leading bytes through
		 * scratch to advance abs_dst onto the next page boundary,
		 * then DMA the rest direct. */
		size_t dst_pg_off = (uintptr_t)abs_dst & (NVME_PRP_PAGE - 1);
		size_t head_bytes = 0;
		if (dst_pg_off && remaining)
			head_bytes = NVME_PRP_PAGE - dst_pg_off;
		if (head_bytes > remaining)
			head_bytes = remaining;
		if (head_bytes) {
			rc = ensure_bounce_buf(d);
			if (rc < 0)
				return rc;

			uint64_t head_lbas = (head_bytes + lba_nbytes - 1) /
			                     lba_nbytes;
			uint16_t nlb = (uint16_t)(head_lbas - 1);
			rc = xnvme_nvm_read(&cmd, d->nsid, cur_slba, nlb,
			                    d->bounce_buf, NULL);
			if (rc || xnvme_cmd_ctx_cpl_status(&cmd)) {
				fprintf(stderr, "aisio HEAD: rc=%d sc=%u sct=%u "
				                "slba=%llu nlb=%u dst_pg_off=%zu "
				                "head_bytes=%zu\n",
				        rc, cmd.cpl.status.sc, cmd.cpl.status.sct,
				        (unsigned long long)cur_slba, nlb,
				        dst_pg_off, head_bytes);
				return -EIO;
			}

			cudaError_t cerr = cudaMemcpy(abs_dst, d->bounce_buf,
			                              head_bytes,
			                              cudaMemcpyDefault);
			if (cerr != cudaSuccess)
				return -EIO;

			cur_slba += head_lbas;
			abs_dst += head_bytes;
			remaining -= head_bytes;
			total_transferred += head_bytes;
		}

		/* MIDDLE: aligned DMA chunks straight into the caller's
		 * buffer. abs_dst is now NVME_PRP_PAGE-aligned (or
		 * remaining is zero / sub-LBA, in which case the loop is
		 * skipped). */
		uint64_t tail_bytes = remaining % lba_nbytes;
		uint64_t middle_lbas = (remaining - tail_bytes) / lba_nbytes;
		uint64_t lbas_done = 0;
		while (lbas_done < middle_lbas) {
			uint64_t chunk_lbas = XNVME_MIN_U64(
			        middle_lbas - lbas_done, max_chunk_lbas);
			chunk_lbas = XNVME_MIN_U64(chunk_lbas, NVME_MAX_NLB);

			uint16_t nlb = (uint16_t)(chunk_lbas - 1);
			uint8_t *dst_chunk = abs_dst + lbas_done * lba_nbytes;

			rc = xnvme_nvm_read(&cmd, d->nsid, cur_slba + lbas_done,
			                    nlb, dst_chunk, NULL);
			if (rc || xnvme_cmd_ctx_cpl_status(&cmd)) {
				fprintf(stderr, "aisio MID: rc=%d sc=%u sct=%u "
				                "slba=%llu nlb=%u dst=%p\n",
				        rc, cmd.cpl.status.sc, cmd.cpl.status.sct,
				        (unsigned long long)(cur_slba + lbas_done),
				        nlb, (void *)dst_chunk);
				return -EIO;
			}

			lbas_done += chunk_lbas;
			total_transferred += chunk_lbas * lba_nbytes;
		}
		cur_slba += middle_lbas;
		abs_dst += middle_lbas * lba_nbytes;
		remaining -= middle_lbas * lba_nbytes;

		/* TAIL: trailing partial LBA (file size not LBA-multiple).
		 * The FS allocates a full block, so the LBA is on disk;
		 * only the first tail_bytes within it are file content. */
		if (tail_bytes) {
			rc = ensure_bounce_buf(d);
			if (rc < 0)
				return rc;

			rc = xnvme_nvm_read(&cmd, d->nsid, cur_slba, 0,
			                    d->bounce_buf, NULL);
			if (rc || xnvme_cmd_ctx_cpl_status(&cmd)) {
				fprintf(stderr, "aisio TAIL: rc=%d sc=%u sct=%u "
				                "slba=%llu tail_bytes=%llu\n",
				        rc, cmd.cpl.status.sc, cmd.cpl.status.sct,
				        (unsigned long long)cur_slba,
				        (unsigned long long)tail_bytes);
				return -EIO;
			}

			cudaError_t cerr = cudaMemcpy(abs_dst, d->bounce_buf,
			                              tail_bytes,
			                              cudaMemcpyDefault);
			if (cerr != cudaSuccess)
				return -EIO;

			total_transferred += tail_bytes;
		}
	}

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

	for (int i = 0; i < drv->buf_count; i++) {
		struct buf_entry *e = &drv->bufs[i];
		if (e->owned)
			xnvme_buf_free(drv->xdev, (void *)e->base);
		else
			xnvme_mem_unmap(drv->xdev, (void *)e->base);
	}
	drv->buf_count = 0;

	if (drv->bounce_buf)
		xnvme_buf_free(drv->xdev, drv->bounce_buf);

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
	e->owned = true;
	return buf;
}

void
ds_file_free(void *buf)
{
	if (!drv || !buf)
		return;

	for (int i = 0; i < drv->buf_count; i++) {
		if (drv->bufs[i].base == buf) {
			if (!drv->bufs[i].owned)
				return;
			xnvme_buf_free(drv->xdev, buf);
			drv->bufs[i] = drv->bufs[drv->buf_count - 1];
			drv->buf_count--;
			return;
		}
	}
}

ds_file_error_t
ds_file_buf_register(const void *buf_base, size_t size, int flags)
{
	(void)flags;

	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!drv->xdev)
		return ds_file_err(DS_FILE_DEVICE_NOT_FOUND);
	if (!buf_base || !size)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (int i = 0; i < drv->buf_count; i++) {
		if (drv->bufs[i].base == buf_base)
			return ds_file_err(DS_FILE_MEMORY_ALREADY_REGISTERED);
	}
	if (drv->buf_count >= MAX_BUF_ENTRIES)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	int rc = xnvme_mem_map(drv->xdev, (void *)buf_base, size);
	if (rc < 0) {
		fprintf(stderr,
		        "ds_file_buf_register: xnvme_mem_map(%p, %zu) rc=%d\n",
		        buf_base, size, rc);
		return ds_file_err(DS_FILE_DEVICE_DRIVER_ERROR);
	}

	struct buf_entry *e = &drv->bufs[drv->buf_count++];
	e->base = buf_base;
	e->length = size;
	e->owned = false;
	return ds_file_ok();
}

ds_file_error_t
ds_file_buf_deregister(const void *buf_base)
{
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!drv->xdev)
		return ds_file_err(DS_FILE_DEVICE_NOT_FOUND);
	if (!buf_base)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	for (int i = 0; i < drv->buf_count; i++) {
		if (drv->bufs[i].base == buf_base) {
			if (drv->bufs[i].owned)
				return ds_file_err(DS_FILE_INVALID_VALUE);
			drv->bufs[i] = drv->bufs[drv->buf_count - 1];
			drv->buf_count--;
			xnvme_mem_unmap(drv->xdev, (void *)buf_base);
			return ds_file_ok();
		}
	}
	return ds_file_err(DS_FILE_MEMORY_NOT_REGISTERED);
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
	if (n < 0) {
		fprintf(stderr,
		        "ds_file_read: aisio_read_extents(size=%zu, off=%ld) rc=%zd\n",
		        size, (long)file_offset, n);
		return -(ssize_t)DS_FILE_DEVICE_DRIVER_ERROR;
	}

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
