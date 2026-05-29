#define _GNU_SOURCE

#include "ds_bounce_ctx.h"

#include <cuda.h>
#include <libxnvme.h>

#include <stdlib.h>
#include <string.h>

int
ds_bounce_ctx_init(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev,
                   uint32_t cap)
{
	memset(ctx, 0, sizeof(*ctx));

	/* DEVICEMAP'd so the I/O thread fills it via host stores and
	 * ds_bounce_kernel reads it on the GPU. Zero-init keeps a bounce-free
	 * op (count == 0) kernel-safe. */
	void *plan_host = NULL;
	if (cuMemHostAlloc(&plan_host, sizeof(struct ds_bounce_plan),
	                   CU_MEMHOSTALLOC_DEVICEMAP |
	                           CU_MEMHOSTALLOC_PORTABLE) != CUDA_SUCCESS)
		return -1;
	CUdeviceptr plan_dev = 0;
	if (cuMemHostGetDevicePointer(&plan_dev, plan_host, 0) !=
	    CUDA_SUCCESS) {
		cuMemFreeHost(plan_host);
		return -1;
	}
	memset(plan_host, 0, sizeof(struct ds_bounce_plan));

	/* cap is fixed for the context's lifetime: the worst case is one tail
	 * bounce per op and the per-stream gate serialises ops, so the slots
	 * never need to grow. */
	void *buf = xnvme_buf_alloc(xdev, (size_t)cap * NVME_PRP_PAGE);
	if (!buf) {
		cuMemFreeHost(plan_host);
		return -1;
	}
	void *eh = NULL;
	if (cuMemHostAlloc(&eh, (size_t)cap * sizeof(struct ds_bounce_entry),
	                   CU_MEMHOSTALLOC_DEVICEMAP |
	                           CU_MEMHOSTALLOC_PORTABLE) != CUDA_SUCCESS) {
		xnvme_buf_free(xdev, buf);
		cuMemFreeHost(plan_host);
		return -1;
	}
	CUdeviceptr ed = 0;
	if (cuMemHostGetDevicePointer(&ed, eh, 0) != CUDA_SUCCESS) {
		cuMemFreeHost(eh);
		xnvme_buf_free(xdev, buf);
		cuMemFreeHost(plan_host);
		return -1;
	}

	ctx->plan_host = (struct ds_bounce_plan *)plan_host;
	ctx->plan_dev = plan_dev;
	ctx->buf = buf;
	ctx->entries_host = (struct ds_bounce_entry *)eh;
	ctx->entries_dev = ed;
	ctx->plan_host->entries = (uint64_t)ed;
	ctx->cap = cap;
	return 0;
}

void
ds_bounce_ctx_free(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev)
{
	if (ctx->buf) {
		xnvme_buf_free(xdev, ctx->buf);
		ctx->buf = NULL;
	}
	if (ctx->entries_host) {
		cuMemFreeHost(ctx->entries_host);
		ctx->entries_host = NULL;
	}
	ctx->entries_dev = 0;
	ctx->cap = 0;
	if (ctx->plan_host) {
		cuMemFreeHost(ctx->plan_host);
		ctx->plan_host = NULL;
	}
	ctx->plan_dev = 0;
}
