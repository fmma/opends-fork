/*
 * Bounce kernel interface shared between opends_aisio.c (host) and
 * ds_bounce_kernel.cu (device). The async path issues cuStreamWaitValue32 on
 * the user's stream to hold it off until the I/O thread writes the
 * DMA-done gate, then enqueues this kernel to perform the
 * device-to-device copies that close out misaligned head/tail spans.
 */

#ifndef DS_BOUNCE_KERNEL_H
#define DS_BOUNCE_KERNEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ds_bounce_entry {
	uint64_t dst; /* GPU device pointer. */
	uint64_t src; /* GPU device pointer (bounce slot). */
	uint32_t n_bytes;
	uint32_t _pad;
};

/* Fixed-size header the kernel launch points at. The entry array is a
 * separate growable devicemapped allocation referenced by entries, so
 * an op can carry an arbitrary number of head/tail bounces (up to 2 per
 * extent) without resizing this header or moving its device pointer.
 * The I/O thread fills count and entries before raising the DMA-done
 * gate; the kernel reads them on the user's stream once the gate
 * clears. */
struct ds_bounce_plan {
	uint32_t count;
	uint32_t _pad;
	uint64_t entries; /* device pointer to ds_bounce_entry[count]. */
};

/* Launch the bounce kernel on the given stream. The caller is
 * responsible for issuing cuStreamWaitValue32 ahead of this so the
 * stream is held off until DMA is done and plan_dev is published. */
int ds_bounce_launch(void *plan_dev, void *stream);

#ifdef __cplusplus
}
#endif

#endif
