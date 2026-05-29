/*
 * Per-stream bounce context for the aisio async path: a contiguous GPU
 * buffer of page-sized slots, a devicemapped ds_bounce_entry array
 * consumed by ds_bounce_kernel, and a stable pinned-mapped plan header.
 * Slots are reused across ops; the per-stream gate serialises the stream,
 * so a drained op's slots are free before the next op claims them.
 *
 * Allocation helpers call CUDA APIs that do a device-wide sync, so they
 * must run on a host thread: on the aisio I/O thread they would deadlock
 * waiting on the thread's own gated stream.
 */

#ifndef DS_BOUNCE_CTX_H
#define DS_BOUNCE_CTX_H

#include "ds_bounce_kernel.h"

#include <cuda.h>
#include <stdint.h>

/* Samsung 990 PRO rejects non-page-aligned PRP1 with status 0x13, so each
 * slot is one PRP page and dst spans are page-aligned. */
#define NVME_PRP_PAGE 4096

struct xnvme_dev;

/* cap slots and a matching entry array, both fixed at init. */
struct ds_bounce_ctx {
	void *buf;
	struct ds_bounce_entry *entries_host;
	CUdeviceptr entries_dev;
	uint32_t cap;
	struct ds_bounce_plan *plan_host;
	CUdeviceptr plan_dev;
};

/* Allocate the plan header, bounce buffer, and entry array for cap slots.
 * Returns 0 on success, -1 on failure (ctx left zeroed). */
int ds_bounce_ctx_init(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev,
                       uint32_t cap);

void ds_bounce_ctx_free(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev);

/* Caller guarantees i < cap. */
static inline void *
ds_bounce_ctx_buf(const struct ds_bounce_ctx *ctx, uint32_t i)
{
	return (char *)ctx->buf + (size_t)i * NVME_PRP_PAGE;
}

static inline void
ds_bounce_ctx_plan_memcpy(struct ds_bounce_ctx *ctx, uint32_t i, uint64_t dst,
                          uint64_t src, uint32_t n_bytes)
{
	ctx->entries_host[i].dst = dst;
	ctx->entries_host[i].src = src;
	ctx->entries_host[i].n_bytes = n_bytes;
	ctx->entries_host[i]._pad = 0;
}

static inline void
ds_bounce_ctx_finalize_plan(struct ds_bounce_ctx *ctx, uint32_t count)
{
	ctx->plan_host->count = count;
}

#endif
