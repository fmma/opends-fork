/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Bounce copy descriptor shared between opends_aisio.c (host) and the
 * vendor kernel TU (device). The async path issues a stream wait-value on
 * the user's stream to hold it off until the I/O thread writes the DMA-done
 * gate, then enqueues the copy that closes out the misaligned tail span.
 */

#ifndef DS_BOUNCE_KERNEL_H
#define DS_BOUNCE_KERNEL_H

#include <stdint.h>

/* The single device-to-device copy the kernel performs, ordered on the
 * user's stream. The I/O thread fills it (devicemapped: host stores, GPU
 * reads) before raising the DMA-done gate; the kernel reads it once the
 * gate clears. n_bytes == 0 no-ops, so a zero-init descriptor is safe. */
struct ds_bounce_copy {
	uint64_t dst;     /* device pointer. */
	uint64_t src;     /* device pointer (bounce slot). */
	uint32_t n_bytes; /* 0 = no-op. */
	uint32_t _pad;
};

#endif
