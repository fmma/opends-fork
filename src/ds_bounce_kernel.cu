/*
 * Bounce copy kernel for the aisio async path. The host enqueues it
 * behind the cuStreamWaitValue32 gate (see ds_bounce_kernel.h), so by
 * launch time NVMe has filled every bounce slot and plan->count is
 * published. One block per entry, threads stride bytes within an entry;
 * blocks stride over plan->count so one launch shape fits any count,
 * including 0 (every block falls through).
 */

#include "ds_bounce_kernel.h"

#include <cuda_runtime.h>
#include <stdint.h>

/* Fixed launch width: enough blocks to run the common head/tail counts
 * one-per-block; higher counts loop via the block stride. */
#define DS_BOUNCE_LAUNCH_BLOCKS 64

static __global__ void
ds_bounce_kernel(const struct ds_bounce_plan *plan)
{
	uint32_t count = plan->count;
	const struct ds_bounce_entry *e =
	        (const struct ds_bounce_entry *)(uintptr_t)plan->entries;

	for (uint32_t i = blockIdx.x; i < count; i += gridDim.x) {
		uint8_t *dst = (uint8_t *)(uintptr_t)e[i].dst;
		const uint8_t *src = (const uint8_t *)(uintptr_t)e[i].src;
		uint32_t n = e[i].n_bytes;

		for (uint32_t off = threadIdx.x; off < n; off += blockDim.x)
			dst[off] = src[off];
	}
}

extern "C" int
ds_bounce_launch(void *plan_dev, void *stream)
{
	cudaStream_t cs = (cudaStream_t)stream;
	ds_bounce_kernel<<<DS_BOUNCE_LAUNCH_BLOCKS, 128, 0, cs>>>(
	        (const struct ds_bounce_plan *)plan_dev);
	return (int)cudaGetLastError();
}
