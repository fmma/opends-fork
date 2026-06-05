/*
 * ds_file_aisio.c - aisio backend for raw-NVMe direct storage.
 *
 * Reads go straight from an NVMe device into GPU memory via xNVMe's upcie-cuda
 * backend (PCIe P2P DMA, no filesystem in the path). HOMI owns the device: it
 * resolves a registered file's path to device extents (homic_get_extents) and
 * hands out the I/O qpair the reads are driven over.
 *
 * Requires: libxnvme and the CUDA toolkit. The NVMe kernel driver must be
 * unbound from the target device before ds_file_driver_open runs.
 *
 * Write and batch async paths report DS_FILE_IO_NOT_SUPPORTED.
 */

#define _GNU_SOURCE

#include "cu_stream_map.h"
#include "ds_bounce_kernel.h"
#include "ds_bounce_ctx.h"
#include "ds_file_internal.h"
#include "ds_extent.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <homic.h>
#include <libxnvme.h>

/* HOMI daemon endpoint and the device it owns, configured by the consumer.
 * The aisio backend attaches I/O qpairs from this daemon and reads files on a
 * filesystem mounted over a block device backed by the same NVMe controller. */
#define ENV_HOMI_SOCKET "OPENDS_HOMI_SOCKET"
#define ENV_HOMI_DEV "OPENDS_HOMI_DEV"
#define ENV_HOMI_MNT "OPENDS_HOMI_MNT"
#define DEFAULT_HOMI_SOCKET "/run/homi/homi.sock"
#define AISIO_ATTACH_QPAIRS 4 ///< sync (idx 0) + sync_queue + async_queue + slack
#define MAX_BUF_ENTRIES 8192
#define DEFAULT_BOUNCE_SIZE (128 * 1024)
#define NVME_MAX_NLB 65536
#define DS_BOUNCE_INIT_CAP 4
#define AISIO_QUEUE_DEPTH 32
/* In attach mode the async queue runs on a HOMI-handed qpair whose ring depth
 * is fixed by the owner (upcie caps a qpair at 1024 entries). NVMe needs one
 * free slot, so the queue capacity must stay below that ring depth. */
#define AISIO_ASYNC_QUEUE_DEPTH 512

#define MAX_STREAMS 8192
#define STREAM_WORDS_BYTES (MAX_STREAMS * sizeof(uint32_t))

#define FILE_OP_QUEUE_SIZE 1024
#define FILE_OP_QUEUE_MASK (FILE_OP_QUEUE_SIZE - 1)

/* 2x MAX_STREAMS keeps load factor <= 50%, ~1 probe per
 * lookup on the read_async hot path. */
#define STREAM_MAP_SIZE (2 * MAX_STREAMS)
#define STREAM_MAP_MASK (STREAM_MAP_SIZE - 1)

enum file_op_state {
	FILE_OP_FREE = 0,
	FILE_OP_PENDING = 1,
	FILE_OP_IN_FLIGHT = 2,
};

struct buf_entry {
	const void *base;
	size_t length;
	bool owned; /* true: xnvme_buf_alloc; false: xnvme_mem_map. */
};

struct aisio_handle {
	int fd;
	struct ds_extent *extents; ///< file->LBA extents from HOMI, sorted by file_offset
	uint32_t extent_count;
};

struct opends_stream {
	CUstream cu_stream;
	uint32_t *gate; /* pinned-mapped */
	CUdeviceptr gate_dptr;
	uint32_t next_seq; /* caller-owned (single host thread). */

	struct ds_bounce_ctx bounce_ctx;
};

struct file_op {
	/* Pointers are dereferenced by the I/O thread at execution time,
	 * so the caller may mutate them between submit and GPU wakeup. */
	struct aisio_handle *h;
	void *buf_base;
	size_t *size_p;
	off_t *file_offset_p;
	off_t *buf_offset_p;
	ssize_t *bytes_read_p;

	struct opends_stream *opends_stream;
	uint32_t seq;
	enum file_op_state state;

	/* The op completes when both counters reach zero. n_bounces is also
	 * written into plan_host->count by finalize_file_op so the kernel runs
	 * over exactly the slots populated for this op. */
	int chunks_remaining;
	int bounces_outstanding;
	int n_bounces;
	size_t bytes_acc;
	int err;
};

struct read_cursor {
	struct xnvme_queue *queue;
	void **bounce_buf_p; /* indirected for lazy allocation. */
	uint64_t cur_slba;
	uint8_t *abs_dst;
	size_t remaining;
};

struct aisio_driver {
	char dev_uri[64];       ///< NVMe device (BDF) the HOMI daemon owns
	char mnt[256];          ///< Mount root stripped to form FS-root-relative paths
	char *attach_descpath;  ///< HOMI-served qpair attach descriptor file
	struct xnvme_dev *xdev;
	/* xNVMe queues are not thread-safe, so sync (host thread) and async
	 * (I/O thread) each get their own. */
	struct xnvme_queue *sync_queue;
	struct xnvme_queue *async_queue;
	uint32_t nsid;
	uint32_t lba_size;
	uint32_t lba_shift;
	uint32_t mdts_nbytes;
	struct buf_entry bufs[MAX_BUF_ENTRIES];
	int buf_count;

	void *sync_bounce_buf;
	bool async_ready;
	CUcontext cu_ctx;

	struct opends_stream streams[MAX_STREAMS];
	int n_streams;
	void *stream_words_host;
	CUdeviceptr stream_words_dptr;

	struct cu_stream_map_entry stream_map[STREAM_MAP_SIZE];

	/* Caller writes queue_head; I/O thread writes queue_tail. */
	struct file_op file_op_queue[FILE_OP_QUEUE_SIZE];
	uint32_t queue_head;
	uint32_t queue_tail;

	pthread_t io_thread;
	bool stop;
};

static struct aisio_driver *drv;
static long use_count;

static inline uint64_t
max_u64(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

static int
ensure_bounce_buf(struct aisio_driver *d, void **bounce_buf_p)
{
	if (*bounce_buf_p)
		return 0;
	*bounce_buf_p = xnvme_buf_alloc(d->xdev, NVME_PRP_PAGE);
	return *bounce_buf_p ? 0 : -ENOMEM;
}

static void
completion_cb(struct xnvme_cmd_ctx *ctx, void *opaque)
{
	(void)ctx;
	*(bool *)opaque = true;
}

struct middle_state {
	int outstanding;
	int err_rc;
	struct xnvme_spec_cpl err_cpl;
	uint64_t err_slba;
	uint16_t err_nlb;
};

static void
middle_cb(struct xnvme_cmd_ctx *ctx, void *opaque)
{
	struct middle_state *s = opaque;
	if (xnvme_cmd_ctx_cpl_status(ctx) && !s->err_rc) {
		s->err_rc = -EIO;
		s->err_cpl = ctx->cpl;
		s->err_slba = ctx->cmd.nvm.slba;
		s->err_nlb = (uint16_t)ctx->cmd.nvm.nlb;
	}
	xnvme_queue_put_cmd_ctx(ctx->async.queue, ctx);
	s->outstanding--;
}

/* Resolve a registered file's extents through homic_get_extents: map the fd
 * back to a path via /proc/self/fd, strip the mount root to a filesystem-
 * relative path, and pass it to the daemon, which returns the device extents. */
static int
aisio_resolve_extents(struct aisio_driver *d, int fd, struct ds_extent **out,
                      uint32_t *out_n)
{
	char fdlink[64];
	char abspath[PATH_MAX];

	snprintf(fdlink, sizeof(fdlink), "/proc/self/fd/%d", fd);
	ssize_t len = readlink(fdlink, abspath, sizeof(abspath) - 1);
	if (len < 0)
		return -errno;
	abspath[len] = '\0';

	const char *rel = abspath;
	size_t mlen = strlen(d->mnt);
	if (mlen && strncmp(abspath, d->mnt, mlen) == 0)
		rel = abspath + mlen;
	while (*rel == '/')
		rel++;

	struct homic_extent *hx = NULL;
	uint32_t n = 0;
	int rc = homic_get_extents(d->dev_uri, rel, &hx, &n);
	if (rc < 0)
		return rc;

	struct ds_extent *ex = calloc(n ? n : 1, sizeof(*ex));
	if (!ex) {
		free(hx);
		return -ENOMEM;
	}
	for (uint32_t i = 0; i < n; i++) {
		ex[i].file_offset = hx[i].file_offset;
		ex[i].slba = hx[i].slba;
		ex[i].length = hx[i].length;
	}
	free(hx);

	*out = ex;
	*out_n = n;
	return 0;
}

static int
open_device(struct aisio_driver *d, int fd)
{
	(void)fd;

	int rc = homic_attach_qpair(d->dev_uri, AISIO_ATTACH_QPAIRS,
	                            &d->attach_descpath);
	if (rc < 0) {
		fprintf(stderr, "aisio open_device: homic_attach_qpair(%s) rc=%d\n",
		        d->dev_uri, rc);
		return rc;
	}

	setenv("XNVME_UPCIE_ATTACH", d->attach_descpath, 1);
	struct xnvme_opts opts = xnvme_opts_default();
	opts.be = "upcie-cuda";

	d->xdev = xnvme_dev_open(d->dev_uri, &opts);
	unsetenv("XNVME_UPCIE_ATTACH");
	if (!d->xdev)
		return -EIO;

	const struct xnvme_geo *geo = xnvme_dev_get_geo(d->xdev);
	d->nsid = xnvme_dev_get_nsid(d->xdev);
	d->lba_size = geo->lba_nbytes ? geo->lba_nbytes : geo->nbytes;
	d->mdts_nbytes =
	        geo->mdts_nbytes ? geo->mdts_nbytes : DEFAULT_BOUNCE_SIZE;

	/* xnvme_dev_open returns success with zeroed geometry if the controller
	 * wasn't fully identified (e.g. opened after the kernel nvme driver
	 * shut it down without a PCI reset). */
	if (d->lba_size == 0) {
		fprintf(stderr,
		        "aisio open_device: zero geometry from xnvme_dev_open "
		        "(lba_nbytes=%u nbytes=%u mdts_nbytes=%u); "
		        "controller likely needs a PCI reset before this "
		        "open\n",
		        geo->lba_nbytes, geo->nbytes, geo->mdts_nbytes);
		xnvme_dev_close(d->xdev);
		d->xdev = NULL;
		return -EIO;
	}
	if (d->lba_size & (d->lba_size - 1)) {
		fprintf(stderr,
		        "aisio open_device: lba_size=%u is not a power of 2\n",
		        d->lba_size);
		xnvme_dev_close(d->xdev);
		d->xdev = NULL;
		return -EIO;
	}
	d->lba_shift = (uint32_t)__builtin_ctz(d->lba_size);

	rc = xnvme_queue_init(d->xdev, AISIO_QUEUE_DEPTH, 0, &d->sync_queue);
	if (rc < 0) {
		fprintf(stderr, "aisio open_device: xnvme_queue_init rc=%d\n",
		        rc);
		xnvme_dev_close(d->xdev);
		d->xdev = NULL;
		return rc;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/*  Sync I/O                                                           */
/* ------------------------------------------------------------------ */

/* LBA-rounded NVMe read into the bounce buffer, then synchronous cudaMemcpy to
 * c->abs_dst, and advance the cursor. Used for the sub-LBA tail: NVMe reads are
 * LBA-granular, so the final partial LBA is read whole into the bounce buffer
 * and only nbytes are copied out. Sync path only; the async path uses
 * submit_read_bounce. */
static int
read_bounce(struct aisio_driver *d, struct read_cursor *c, size_t nbytes)
{
	uint64_t nlbas = (nbytes + (d->lba_size - 1)) >> d->lba_shift;
	uint16_t nlb = (uint16_t)(nlbas - 1);

	if (ensure_bounce_buf(d, c->bounce_buf_p) < 0)
		return -1;
	void *bounce_buf = *c->bounce_buf_p;

	struct xnvme_cmd_ctx *ctx;
	for (;;) {
		ctx = xnvme_queue_get_cmd_ctx(c->queue);
		if (ctx)
			break;
		if (xnvme_queue_poke(c->queue, 0) < 0)
			return -1;
	}

	bool done = false;
	xnvme_cmd_ctx_set_cb(ctx, completion_cb, &done);
	if (xnvme_nvm_read(ctx, d->nsid, c->cur_slba, nlb, bounce_buf, NULL)) {
		xnvme_queue_put_cmd_ctx(c->queue, ctx);
		return -1;
	}
	while (!done)
		(void)xnvme_queue_poke(c->queue, 0);
	int status = xnvme_cmd_ctx_cpl_status(ctx);
	xnvme_queue_put_cmd_ctx(c->queue, ctx);
	if (status)
		return -1;

	if (cudaMemcpy(c->abs_dst, bounce_buf, nbytes, cudaMemcpyDefault) !=
	    cudaSuccess)
		return -1;

	c->cur_slba += nlbas;
	c->abs_dst += nbytes;
	c->remaining -= nbytes;
	return 0;
}

static int
sync_read_middle(struct aisio_driver *d, struct read_cursor *c,
                 uint64_t middle_lbas)
{
	uint32_t lba_nbytes = d->lba_size;
	uint64_t max_chunk_lbas = d->mdts_nbytes >> d->lba_shift;
	struct middle_state mst = {0};
	uint64_t lbas_done = 0;
	while (lbas_done < middle_lbas) {
		uint64_t chunk_lbas =
		        XNVME_MIN_U64(middle_lbas - lbas_done, max_chunk_lbas);
		chunk_lbas = XNVME_MIN_U64(chunk_lbas, NVME_MAX_NLB);

		uint16_t nlb = (uint16_t)(chunk_lbas - 1);
		uint8_t *dst_chunk = c->abs_dst + lbas_done * lba_nbytes;

		/* xnvme's ctx pool is capacity+1, but submit rejects -EBUSY at
		 * outstanding == capacity. */
		while (mst.outstanding >= AISIO_QUEUE_DEPTH) {
			int r = xnvme_queue_poke(c->queue, 0);
			if (r < 0) {
				mst.err_rc = r;
				break;
			}
		}
		if (mst.err_rc)
			break;

		struct xnvme_cmd_ctx *ctx = xnvme_queue_get_cmd_ctx(c->queue);
		if (!ctx) {
			mst.err_rc = -errno;
			break;
		}

		xnvme_cmd_ctx_set_cb(ctx, middle_cb, &mst);
		int srq = xnvme_nvm_read(ctx, d->nsid, c->cur_slba + lbas_done,
		                         nlb, dst_chunk, NULL);
		if (srq) {
			xnvme_queue_put_cmd_ctx(c->queue, ctx);
			mst.err_rc = srq;
			break;
		}
		mst.outstanding++;
		lbas_done += chunk_lbas;
	}

	while (mst.outstanding > 0) {
		/* middle_cb references stack-local mst; xnvme has no cancel, so
		 * we must wait for every callback even on error. */
		int r = xnvme_queue_poke(c->queue, 0);
		if (r < 0 && !mst.err_rc)
			mst.err_rc = r;
	}

	if (mst.err_rc) {
		fprintf(stderr,
		        "aisio middle: rc=%d sc=%u sct=%u slba=%llu nlb=%u\n",
		        mst.err_rc, mst.err_cpl.status.sc,
		        mst.err_cpl.status.sct,
		        (unsigned long long)mst.err_slba, mst.err_nlb);
		return -EIO;
	}

	c->cur_slba += middle_lbas;
	c->abs_dst += middle_lbas * lba_nbytes;
	c->remaining -= middle_lbas * lba_nbytes;
	return 0;
}

static ssize_t
read_extents(struct aisio_driver *d, struct aisio_handle *h, void *dst,
             size_t size, off_t file_offset)
{
	uint64_t req_start = (uint64_t)file_offset;
	if (size > UINT64_MAX - req_start)
		return -EINVAL;
	uint64_t req_end = req_start + size;

	const struct ds_extent *extents = h->extents;
	uint32_t extent_count = h->extent_count;
	int rc;

	uint32_t lba_nbytes = d->lba_size;
	uint32_t lba_shift = d->lba_shift;
	uint32_t lba_mask = d->lba_size - 1;
	if ((d->mdts_nbytes >> lba_shift) == 0) {
		fprintf(stderr,
		        "aisio prelude: max_chunk_lbas=0 (mdts=%u lba=%u)\n",
		        d->mdts_nbytes, lba_nbytes);
		return -EINVAL;
	}

	struct read_cursor c = {
	        .queue = d->sync_queue,
	        .bounce_buf_p = &d->sync_bounce_buf,
	};
	size_t total_transferred = 0;

	for (uint32_t i = 0; i < extent_count; i++) {
		const struct ds_extent *e = &extents[i];

		uint64_t ext_start = e->file_offset;
		uint64_t ext_end = ext_start + e->length;

		/* Extents are sorted by file_offset. */
		if (ext_start >= req_end)
			break;

		uint64_t span_start = max_u64(req_start, ext_start);
		uint64_t span_end = XNVME_MIN_U64(req_end, ext_end);
		if (span_start >= span_end)
			continue;

		uint64_t off_in_ext = span_start - ext_start;
		size_t buf_off = span_start - req_start;

		/* FS extents land on FS-block boundaries (LBA multiples). */
		if (off_in_ext & lba_mask)
			return -EINVAL;

		c.abs_dst = (uint8_t *)dst + buf_off;
		c.remaining = span_end - span_start;
		c.cur_slba = e->slba + (off_in_ext >> lba_shift);

		/* xnvme reads are LBA-granular; sub-LBA dst alignment would
		 * need a full bounce of arbitrary spans. A page-misaligned (but
		 * LBA-aligned) dst is fine: PRP1 carries the sub-page offset.
		 */
		if ((uintptr_t)c.abs_dst & lba_mask)
			return -EINVAL;

		size_t tail_bytes = c.remaining & lba_mask;
		uint64_t middle_lbas = (c.remaining - tail_bytes) >> lba_shift;
		size_t middle_bytes = middle_lbas * lba_nbytes;
		rc = sync_read_middle(d, &c, middle_lbas);
		if (rc)
			return rc;
		total_transferred += middle_bytes;

		if (tail_bytes) {
			if (read_bounce(d, &c, tail_bytes) < 0)
				return -EIO;
			total_transferred += tail_bytes;
		}
	}

	return (ssize_t)total_transferred;
}

/* ------------------------------------------------------------------ */
/*  Async I/O                                                          */
/* ------------------------------------------------------------------ */

static void
chunk_cb(struct xnvme_cmd_ctx *ctx, void *opaque)
{
	struct file_op *op = opaque;
	if (xnvme_cmd_ctx_cpl_status(ctx)) {
		op->err = DS_FILE_DEVICE_DRIVER_ERROR;
	} else {
		uint32_t lba_size = drv->lba_size;
		op->bytes_acc += ((uint64_t)ctx->cmd.nvm.nlb + 1) * lba_size;
	}
	op->chunks_remaining--;
	xnvme_queue_put_cmd_ctx(ctx->async.queue, ctx);
}

/* Submit middle chunks and return; chunk_cb credits op->bytes_acc as each chunk
 * completes. Head and tail bounces are handled inline by the caller via
 * read_bounce. */
static int
submit_read_middle(struct aisio_driver *d, struct read_cursor *c,
                   struct file_op *op, uint64_t middle_lbas)
{
	uint32_t lba_nbytes = d->lba_size;
	uint64_t max_chunk_lbas = d->mdts_nbytes >> d->lba_shift;
	uint64_t lbas_done = 0;
	while (lbas_done < middle_lbas) {
		uint64_t chunk_lbas =
		        XNVME_MIN_U64(middle_lbas - lbas_done, max_chunk_lbas);
		chunk_lbas = XNVME_MIN_U64(chunk_lbas, NVME_MAX_NLB);

		struct xnvme_cmd_ctx *ctx;
		for (;;) {
			ctx = xnvme_queue_get_cmd_ctx(c->queue);
			if (ctx)
				break;
			xnvme_queue_poke(c->queue, 0);
		}
		xnvme_cmd_ctx_set_cb(ctx, chunk_cb, op);

		uint16_t nlb = (uint16_t)(chunk_lbas - 1);
		uint64_t chunk_slba = c->cur_slba + lbas_done;
		uint8_t *dst_chunk = c->abs_dst + lbas_done * lba_nbytes;

		op->chunks_remaining++;
		int srv = xnvme_nvm_read(ctx, d->nsid, chunk_slba, nlb,
		                         dst_chunk, NULL);
		if (srv == -EBUSY) {
			op->chunks_remaining--;
			xnvme_queue_put_cmd_ctx(c->queue, ctx);
			xnvme_queue_poke(c->queue, 0);
			continue;
		}
		if (srv < 0) {
			op->chunks_remaining--;
			xnvme_queue_put_cmd_ctx(c->queue, ctx);
			op->err = DS_FILE_DEVICE_DRIVER_ERROR;
			return -1;
		}

		lbas_done += chunk_lbas;
	}

	c->cur_slba += middle_lbas;
	c->abs_dst += middle_lbas * lba_nbytes;
	c->remaining -= middle_lbas * lba_nbytes;
	return 0;
}

/* Bounce read completion: record any device error and drop the in-flight
 * count. Unlike chunk_cb, this does not credit bytes_acc; submit_read_bounce
 * already added nbytes at submit time, and on error finalize_file_op reports
 * the error instead of the byte count. */
static void
bounce_cb(struct xnvme_cmd_ctx *ctx, void *opaque)
{
	struct file_op *op = opaque;
	if (xnvme_cmd_ctx_cpl_status(ctx) && !op->err)
		op->err = DS_FILE_DEVICE_DRIVER_ERROR;
	op->bounces_outstanding--;
	xnvme_queue_put_cmd_ctx(ctx->async.queue, ctx);
}

/* Submit a single-LBA bounce read into the next bounce slot and record the
 * (dst, src, n_bytes) tuple in the plan entry the kernel will consume. The dst
 * pointer is the caller's destination address (already resolved through
 * *buf_offset_p + extent offset at this point), so the kernel can stay
 * oblivious to extent geometry. */
static int
submit_read_bounce(struct aisio_driver *d, struct file_op *op, uint8_t *abs_dst,
                   uint64_t cur_slba, size_t nbytes)
{
	struct ds_bounce_ctx *bounce_ctx = &op->opends_stream->bounce_ctx;
	int slot = op->n_bounces;
	/* The per-stream context's initial capacity covers the worst case (one
	 * sub-LBA tail per op), so a slot is always available. Allocating here
	 * would deadlock (CUDA alloc syncs the device while this op's stream is
	 * gated on the I/O thread). Treat an overflow as an internal error. */
	if ((uint32_t)slot >= bounce_ctx->cap) {
		op->err = DS_FILE_INTERNAL_ERROR;
		return -1;
	}
	void *bounce_src = ds_bounce_ctx_buf(bounce_ctx, (uint32_t)slot);

	uint64_t nlbas = (nbytes + (d->lba_size - 1)) >> d->lba_shift;
	uint16_t nlb = (uint16_t)(nlbas - 1);

	for (;;) {
		struct xnvme_cmd_ctx *ctx;
		for (;;) {
			ctx = xnvme_queue_get_cmd_ctx(d->async_queue);
			if (ctx)
				break;
			xnvme_queue_poke(d->async_queue, 0);
		}
		xnvme_cmd_ctx_set_cb(ctx, bounce_cb, op);

		int srv = xnvme_nvm_read(ctx, d->nsid, cur_slba, nlb,
		                         bounce_src, NULL);
		if (srv == -EBUSY) {
			xnvme_queue_put_cmd_ctx(d->async_queue, ctx);
			xnvme_queue_poke(d->async_queue, 0);
			continue;
		}
		if (srv < 0) {
			xnvme_queue_put_cmd_ctx(d->async_queue, ctx);
			op->err = DS_FILE_DEVICE_DRIVER_ERROR;
			return -1;
		}
		break;
	}

	ds_bounce_ctx_plan_memcpy(
	        bounce_ctx, (uint32_t)slot, (uint64_t)(uintptr_t)abs_dst,
	        (uint64_t)(uintptr_t)bounce_src, (uint32_t)nbytes);

	op->bounces_outstanding++;
	op->n_bounces++;
	op->bytes_acc += nbytes;
	return 0;
}

/* Walk the extent list and submit all NVMe ops (middle chunks plus at most one
 * tail bounce) for this op in one pass.
 *
 * Worst case is a single bounce: a page-misaligned dst is DMAed directly (PRP1
 * carries the sub-page offset), so no head bounce is needed, and only req_end
 * can be sub-LBA since interior extent boundaries are LBA-aligned. The
 * per-stream context's initial capacity covers this. */
static void
start_file_op(struct aisio_driver *d, struct file_op *op)
{
	if (d->lba_size == 0) {
		op->err = DS_FILE_INTERNAL_ERROR;
		return;
	}
	if ((d->mdts_nbytes >> d->lba_shift) == 0) {
		op->err = DS_FILE_INVALID_VALUE;
		return;
	}

	size_t size = *op->size_p;
	uint64_t req_start = (uint64_t)*op->file_offset_p;
	if (size > UINT64_MAX - req_start) {
		op->err = DS_FILE_INVALID_VALUE;
		return;
	}
	uint64_t req_end = req_start + size;
	uint8_t *dst_base = (uint8_t *)op->buf_base + *op->buf_offset_p;

	const struct ds_extent *extents = op->h->extents;
	uint32_t extent_count = op->h->extent_count;

	uint32_t lba_shift = d->lba_shift;
	uint32_t lba_mask = d->lba_size - 1;

	for (uint32_t i = 0; i < extent_count; i++) {
		const struct ds_extent *e = &extents[i];
		uint64_t ext_start = e->file_offset;
		uint64_t ext_end = ext_start + e->length;
		if (ext_start >= req_end)
			break;

		uint64_t span_start = max_u64(req_start, ext_start);
		uint64_t span_end = XNVME_MIN_U64(req_end, ext_end);
		if (span_start >= span_end)
			continue;

		uint64_t off_in_ext = span_start - ext_start;
		if (off_in_ext & lba_mask) {
			op->err = DS_FILE_INVALID_VALUE;
			return;
		}
		size_t buf_off = span_start - req_start;
		uint8_t *abs_dst = dst_base + buf_off;
		uint64_t cur_slba = e->slba + (off_in_ext >> lba_shift);
		size_t remaining = span_end - span_start;

		size_t tail_bytes = remaining & lba_mask;
		uint64_t middle_lbas = (remaining - tail_bytes) >> lba_shift;
		if (middle_lbas) {
			struct read_cursor c = {
			        .queue = d->async_queue,
			        .bounce_buf_p = NULL,
			        .cur_slba = cur_slba,
			        .abs_dst = abs_dst,
			        .remaining = remaining,
			};
			if (submit_read_middle(d, &c, op, middle_lbas) < 0)
				return;
			cur_slba = c.cur_slba;
			abs_dst = c.abs_dst;
			remaining = c.remaining;
		}

		if (tail_bytes) {
			if (submit_read_bounce(d, op, abs_dst, cur_slba,
			                       tail_bytes) < 0)
				return;
		}
	}
}

static void
finalize_file_op(struct file_op *op)
{
	struct opends_stream *s = op->opends_stream;

	if (op->err)
		*op->bytes_read_p = -(ssize_t)op->err;
	else
		*op->bytes_read_p = (ssize_t)op->bytes_acc;

	/* The count store must land before the gate store: the kernel polls
	 * *gate, then reads plan->count, so it must not see the new gate with a
	 * stale count. */
	ds_bounce_ctx_finalize_plan(&s->bounce_ctx,
	                            (uint32_t)(op->err ? 0 : op->n_bounces));

	*s->gate = 2 * op->seq + 1;
}

static void
complete_in_flight(struct aisio_driver *d, struct file_op *op)
{
	(void)d;
	finalize_file_op(op);
	op->state = FILE_OP_FREE;
}

/* Take a gate-cleared PENDING op in flight: reset its I/O-thread-local counters
 * and submit all of its NVMe ops in one pass. */
static void
dispatch_pending(struct aisio_driver *d, struct file_op *op)
{
	op->chunks_remaining = 0;
	op->bounces_outstanding = 0;
	op->n_bounces = 0;
	op->bytes_acc = 0;
	op->err = 0;
	op->state = FILE_OP_IN_FLIGHT;
	start_file_op(d, op);
}

/* Complete an IN_FLIGHT op once both its middle-chunk and bounce completions
 * have landed. */
static void
reap_in_flight(struct aisio_driver *d, struct file_op *op)
{
	if (op->chunks_remaining == 0 && op->bounces_outstanding == 0)
		complete_in_flight(d, op);
}

static void *
io_thread_main(void *arg)
{
	struct aisio_driver *d = arg;
	cuCtxSetCurrent(d->cu_ctx);

	for (;;) {
		bool busy = false;

		uint32_t head = d->queue_head;
		for (uint32_t i = d->queue_tail; i != head; i++) {
			struct file_op *op =
			        &d->file_op_queue[i & FILE_OP_QUEUE_MASK];
			switch (op->state) {
			case FILE_OP_PENDING:
				/* The gate is a free-running +1 counter
				 * (WriteValue then this thread's release each
				 * tick it by one), so compare with
				 * serial/cyclic arithmetic to match the
				 * device-side GEQ wait. This makes the sequence
				 * wrap-safe with no 2^31 in-flight bound. */
				if ((int32_t)(*op->opends_stream->gate -
				              2 * op->seq) < 0)
					busy = true;
				else
					dispatch_pending(d, op);
				break;
			case FILE_OP_IN_FLIGHT: reap_in_flight(d, op); break;
			default: break;
			}
		}

		while (d->queue_tail != head) {
			struct file_op *op =
			        &d->file_op_queue[d->queue_tail &
			                          FILE_OP_QUEUE_MASK];
			if (op->state != FILE_OP_FREE)
				break;
			d->queue_tail++;
		}

		if (d->queue_tail != head)
			busy = true;

		if (d->stop && !busy)
			break;

		if (busy) {
			xnvme_queue_poke(d->async_queue, 0);
			sched_yield();
		} else {
			struct timespec ts = {0, 100000}; /* 100 us */
			nanosleep(&ts, NULL);
		}
	}

	return NULL;
}

/* Returns -1 if no CUDA context is current or any allocation fails; async_ready
 * stays false and ds_file_read_async returns an error. */
static int
async_setup(struct aisio_driver *d)
{
	if (cuCtxGetCurrent(&d->cu_ctx) != CUDA_SUCCESS || !d->cu_ctx)
		return -1;

	void *host = NULL;
	if (cuMemHostAlloc(&host, STREAM_WORDS_BYTES,
	                   CU_MEMHOSTALLOC_DEVICEMAP |
	                           CU_MEMHOSTALLOC_PORTABLE) != CUDA_SUCCESS)
		return -1;

	CUdeviceptr dptr = 0;
	if (cuMemHostGetDevicePointer(&dptr, host, 0) != CUDA_SUCCESS) {
		cuMemFreeHost(host);
		return -1;
	}
	memset(host, 0, STREAM_WORDS_BYTES);
	d->stream_words_host = host;
	d->stream_words_dptr = dptr;

	if (xnvme_queue_init(d->xdev, AISIO_ASYNC_QUEUE_DEPTH, 0,
	                     &d->async_queue) < 0) {
		cuMemFreeHost(d->stream_words_host);
		d->stream_words_host = NULL;
		return -1;
	}

	d->stop = false;
	if (pthread_create(&d->io_thread, NULL, io_thread_main, d) != 0) {
		xnvme_queue_term(d->async_queue);
		d->async_queue = NULL;
		cuMemFreeHost(d->stream_words_host);
		d->stream_words_host = NULL;
		return -1;
	}

	d->async_ready = true;
	return 0;
}

static void
async_teardown(struct aisio_driver *d)
{
	if (!d->async_ready)
		return;

	d->stop = true;
	pthread_join(d->io_thread, NULL);

	for (int i = 0; i < d->n_streams; i++)
		ds_bounce_ctx_free(&d->streams[i].bounce_ctx, d->xdev);

	if (d->async_queue) {
		xnvme_queue_term(d->async_queue);
		d->async_queue = NULL;
	}

	if (d->stream_words_host) {
		cuMemFreeHost(d->stream_words_host);
		d->stream_words_host = NULL;
	}

	d->async_ready = false;
}

static struct opends_stream *
opends_stream_get(struct aisio_driver *d, CUstream stream)
{
	int idx = cu_stream_map_get(d->stream_map, STREAM_MAP_MASK, stream);
	if (idx < 0)
		return NULL;
	return &d->streams[idx];
}

/* ------------------------------------------------------------------ */
/*  Driver lifecycle                                                   */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_driver_open(void)
{
	if (drv)
		return ds_file_err(DS_FILE_DRIVER_ALREADY_OPEN);

	const char *dev = getenv(ENV_HOMI_DEV);
	if (!dev || !dev[0]) {
		fprintf(stderr,
		        "aisio: %s must name the NVMe device the HOMI daemon owns\n",
		        ENV_HOMI_DEV);
		return ds_file_err(DS_FILE_FS_SETUP_ERROR);
	}

	struct aisio_driver *d = calloc(1, sizeof(*d));
	if (!d)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	snprintf(d->dev_uri, sizeof(d->dev_uri), "%s", dev);

	const char *mnt = getenv(ENV_HOMI_MNT);
	if (mnt && mnt[0])
		snprintf(d->mnt, sizeof(d->mnt), "%s", mnt);

	const char *sock = getenv(ENV_HOMI_SOCKET);
	int rc = homic_connect(
	        (char *)(sock && sock[0] ? sock : DEFAULT_HOMI_SOCKET));
	if (rc < 0) {
		free(d);
		return ds_file_err(DS_FILE_FS_SETUP_ERROR);
	}

	drv = d;

	int orc = open_device(d, -1);
	if (orc < 0) {
		homic_disconnect();
		free(d->attach_descpath);
		free(d);
		drv = NULL;
		return ds_file_err(DS_FILE_DEVICE_NOT_FOUND);
	}
	async_setup(d);

	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_close(void)
{
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);

	async_teardown(drv);

	for (int i = 0; i < drv->buf_count; i++) {
		struct buf_entry *e = &drv->bufs[i];
		if (e->owned)
			xnvme_buf_free(drv->xdev, (void *)e->base);
		else
			xnvme_mem_unmap(drv->xdev, (void *)e->base);
	}
	drv->buf_count = 0;

	if (drv->sync_bounce_buf)
		xnvme_buf_free(drv->xdev, drv->sync_bounce_buf);

	if (drv->sync_queue)
		xnvme_queue_term(drv->sync_queue);
	if (drv->xdev)
		xnvme_dev_close(drv->xdev);

	homic_detach_qpair();
	homic_disconnect();
	free(drv->attach_descpath);

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
		async_setup(drv);
	}

	struct aisio_handle *h = calloc(1, sizeof(*h));
	if (!h)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);
	h->fd = fd;

	int frc = aisio_resolve_extents(drv, fd, &h->extents, &h->extent_count);
	if (frc < 0) {
		fprintf(stderr, "aisio register: resolve_extents(fd=%d) rc=%d\n", fd, frc);
		free(h);
		return ds_file_err(DS_FILE_FS_SETUP_ERROR);
	}

	*fh = h;
	use_count++;
	return ds_file_ok();
}

void
ds_file_handle_deregister(ds_file_handle_t fh)
{
	if (!fh)
		return;
	free(((struct aisio_handle *)fh)->extents);
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
/*  I/O                                                                */
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
	ssize_t n = read_extents(drv, (struct aisio_handle *)fh, dst, size,
	                         file_offset);
	if (n < 0) {
		fprintf(stderr,
		        "ds_file_read: read_extents(size=%zu, off=%ld) "
		        "rc=%zd\n",
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

ds_file_error_t
ds_file_read_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                   off_t *file_offset_p, off_t *buf_offset_p,
                   ssize_t *bytes_read_p, ds_stream_t stream)
{
	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!fh || !buf_base || !size_p || !file_offset_p || !buf_offset_p ||
	    !bytes_read_p)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	if (!stream)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	if (!drv->async_ready)
		return ds_file_err(DS_FILE_DEVICE_DRIVER_ERROR);

	CUstream cus = (CUstream)stream;
	struct opends_stream *opends_stream = opends_stream_get(drv, cus);
	if (!opends_stream)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	uint32_t head = drv->queue_head;
	while (head - drv->queue_tail >= FILE_OP_QUEUE_SIZE)
		sched_yield();

	uint32_t seq = ++opends_stream->next_seq;
	struct file_op *op = &drv->file_op_queue[head & FILE_OP_QUEUE_MASK];
	op->h = (struct aisio_handle *)fh;
	op->buf_base = buf_base;
	op->size_p = size_p;
	op->file_offset_p = file_offset_p;
	op->buf_offset_p = buf_offset_p;
	op->bytes_read_p = bytes_read_p;
	op->opends_stream = opends_stream;
	op->seq = seq;

	/* Order the host I/O thread against the user's CUDA stream through a
	 * single per-stream gate word (strictly monotonic). The word carries
	 * two phases per op. The main thread's WriteValue(2*seq) is a stream
	 * op, so it fires in stream order once any user-queued host func has
	 * run and the stream is ready for the read. The I/O thread's host store
	 * *gate = 2*seq+1 lands after the I/O finishes and releases the
	 * WaitValue(>= 2*seq+1) below, which blocks the stream until the DMA is
	 * done. */
	/* These stream enqueue calls fail only on an invalid context or handle,
	 * which poisons the stream irrecoverably. Do not roll back next_seq on
	 * failure: a reused seq would let the I/O thread's gate check pass
	 * before the stream is ready. A consumed-but-skipped seq is harmless
	 * since the next op raises the gate to a higher value. */
	if (cuStreamWriteValue32(cus, opends_stream->gate_dptr, 2 * seq,
	                         CU_STREAM_WRITE_VALUE_DEFAULT) != CUDA_SUCCESS)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);
	if (cuStreamWaitValue32(cus, opends_stream->gate_dptr, 2 * seq + 1,
	                        CU_STREAM_WAIT_VALUE_GEQ) != CUDA_SUCCESS)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	op->state = FILE_OP_PENDING;
	drv->queue_head = head + 1;

	/* Launch bounce memcpy kernel unconditionally: offsets resolve behind
	 * the gate, so the bounce count is unknown here, and the kernel no-ops
	 * when it is zero. Launch after publishing so a failed launch is still
	 * drained by the I/O thread (which releases the gate); only this read
	 * is lost. */
	if (ds_bounce_launch(
	            (void *)(uintptr_t)opends_stream->bounce_ctx.plan_dev,
	            (void *)cus) != 0)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	return ds_file_ok();
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
	(void)flags;

	if (!drv)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!stream)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	if (!drv->async_ready)
		return ds_file_err(DS_FILE_DEVICE_DRIVER_ERROR);

	CUstream cus = (CUstream)stream;

	if (cu_stream_map_get(drv->stream_map, STREAM_MAP_MASK, cus) >= 0)
		return ds_file_ok();
	if (drv->n_streams >= MAX_STREAMS)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	int n = drv->n_streams;
	struct opends_stream *opends_stream = &drv->streams[n];
	uint32_t *words = (uint32_t *)drv->stream_words_host;
	opends_stream->cu_stream = cus;
	opends_stream->gate = &words[n];
	opends_stream->gate_dptr =
	        drv->stream_words_dptr + (CUdeviceptr)(n * sizeof(uint32_t));
	*opends_stream->gate = 0;
	opends_stream->next_seq = 0;

	if (ds_bounce_ctx_init(&opends_stream->bounce_ctx, drv->xdev,
	                       DS_BOUNCE_INIT_CAP) < 0)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	if (cu_stream_map_put(drv->stream_map, STREAM_MAP_MASK, cus, n) < 0) {
		ds_bounce_ctx_free(&opends_stream->bounce_ctx, drv->xdev);
		return ds_file_err(DS_FILE_INTERNAL_ERROR);
	}

	drv->n_streams = n + 1;
	return ds_file_ok();
}

ds_file_error_t
ds_file_stream_deregister(ds_stream_t stream)
{
	(void)stream;
	return ds_file_ok();
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
