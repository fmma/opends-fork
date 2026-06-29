/* SPDX-License-Identifier: BSD-3-Clause */
#define _GNU_SOURCE

#include "ds_bounce_ctx.h"

#include <libxnvme.h>

#include <stdlib.h>
#include <string.h>

int
ds_bounce_ctx_init(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev)
{
	memset(ctx, 0, sizeof(*ctx));

	/* DEVICEMAP'd so the I/O thread fills it via host stores and the
	 * kernel reads it on the GPU. Zero-init keeps a bounce-free op
	 * (n_bytes == 0) kernel-safe. */
	void *desc_host = NULL;
	ds_accel_devptr_t desc_dev = 0;
	if (ds_accel->host_alloc_mapped(sizeof(struct ds_bounce_copy), &desc_host,
	                                &desc_dev) < 0)
		return -1;
	memset(desc_host, 0, sizeof(struct ds_bounce_copy));

	/* One slot suffices: at most one tail bounce per op, and the per-stream
	 * gate serialises ops. */
	void *buf = xnvme_buf_alloc(xdev, NVME_PRP_PAGE);
	if (!buf) {
		ds_accel->host_free(desc_host);
		return -1;
	}

	ctx->desc_host = (struct ds_bounce_copy *)desc_host;
	ctx->desc_dev = desc_dev;
	ctx->buf = buf;
	return 0;
}

void
ds_bounce_ctx_free(struct ds_bounce_ctx *ctx, struct xnvme_dev *xdev)
{
	if (ctx->buf) {
		xnvme_buf_free(xdev, ctx->buf);
		ctx->buf = NULL;
	}
	if (ctx->desc_host) {
		ds_accel->host_free(ctx->desc_host);
		ctx->desc_host = NULL;
	}
	ctx->desc_dev = 0;
}
