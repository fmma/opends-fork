/*
 * CUDA bounce copy kernel for the aisio async path. The host enqueues it
 * behind the stream wait-value gate (see ds_bounce_kernel.h), so by launch
 * time NVMe has filled the bounce slot and the descriptor is published.
 * Threads stride bytes within the single copy; n_bytes == 0 falls through,
 * so the launch is safe for a no-op op.
 *
 * Compiled by nvcc; does not include the host ds_accel.h shim.
 */

#include "ds_bounce_kernel.h"

#include <cuda_runtime.h>
#include <stdint.h>

static __global__ void
cuda_bounce_kernel(const struct ds_bounce_copy *desc)
{
	uint8_t *dst = (uint8_t *)(uintptr_t)desc->dst;
	const uint8_t *src = (const uint8_t *)(uintptr_t)desc->src;
	uint32_t n = desc->n_bytes;

	for (uint32_t off = threadIdx.x; off < n; off += blockDim.x)
		dst[off] = src[off];
}

extern "C" int
cuda_copy_stream(uint64_t desc_dev, void *stream)
{
	cuda_bounce_kernel<<<1, 128, 0, (cudaStream_t)stream>>>(
	        (const struct ds_bounce_copy *)(uintptr_t)desc_dev);
	return (int)cudaGetLastError();
}
