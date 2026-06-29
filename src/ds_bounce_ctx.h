/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Per-stream bounce context for the aisio async path: one page-sized GPU
 * bounce slot and a devicemapped copy descriptor consumed by the
 * kernel. The slot is reused across ops; the per-stream gate
 * serialises the stream, so a drained op's slot is free before the next
 * op claims it.
 *
 * Allocation helpers call GPU runtime APIs that do a device-wide sync, so
 * they must run on a host thread: on the aisio I/O thread they would
 * deadlock waiting on the thread's own gated stream.
 */

#ifndef DS_BOUNCE_CTX_H
#define DS_BOUNCE_CTX_H

#include "ds_bounce_kernel.h"
#include "ds_accel.h"

#include <stdint.h>

/* Samsung 990 PRO rejects non-page-aligned PRP1 with status 0x13, so each
 * slot is one PRP page and dst spans are page-aligned. */
#define NVME_PRP_PAGE 4096

struct xnvme_dev;

struct ds_bounce_ctx {
	void *buf;                        /* one PRP page; tail DMA target. */
	struct ds_bounce_copy *desc_host; /* I/O thread writes, kernel reads. */
	ds_accel_devptr_t desc_dev;
};

/* Allocate the bounce slot and copy descriptor. Returns 0 on success, -1
 * on failure (ctx left zeroed). */
int ds_bounce_ctx_init(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev);

void ds_bounce_ctx_free(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev);

static inline void *
ds_bounce_ctx_buf(const struct ds_bounce_ctx *ctx)
{
	return ctx->buf;
}

/* Stage the deferred copy's endpoints (device pointers); finalize sets the
 * size, which arms (n_bytes > 0) or no-ops (0) the copy. */
static inline void
ds_bounce_ctx_stage(struct ds_bounce_ctx *ctx, uint64_t dst, uint64_t src)
{
	ctx->desc_host->dst = dst;
	ctx->desc_host->src = src;
}

static inline void
ds_bounce_ctx_finalize(struct ds_bounce_ctx *ctx, uint32_t n_bytes)
{
	ctx->desc_host->n_bytes = n_bytes;
}

#endif
