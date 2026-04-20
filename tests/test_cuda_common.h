/*
 * Shared CUDA helpers for GPU-backed backend tests (gds, aisio).
 *
 * Provides the test_env callbacks that copy device buffers to host,
 * zero device buffers, and assert that ds_file_alloc returned CUDA
 * device memory.
 */

#ifndef OPENDS_TEST_CUDA_COMMON_H
#define OPENDS_TEST_CUDA_COMMON_H

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>

static inline void *
cuda_buf_to_host(void *dst, const void *src, size_t n)
{
	cudaMemcpy(dst, src, n, cudaMemcpyDeviceToHost);
	return dst;
}

static inline void
cuda_buf_zero(void *buf, size_t n)
{
	cudaMemset(buf, 0, n);
	cudaDeviceSynchronize();
}

static inline void
cuda_check_buffer(const void *buf)
{
	struct cudaPointerAttributes attrs;
	cudaError_t rc = cudaPointerGetAttributes(&attrs, buf);
	if (rc != cudaSuccess) {
		fprintf(stderr, "cudaPointerGetAttributes: %s\n",
		        cudaGetErrorString(rc));
		abort();
	}
	if (attrs.type != cudaMemoryTypeDevice) {
		fprintf(stderr, "ds_file_alloc returned non-device memory "
		                "(type=%d)\n", (int)attrs.type);
		abort();
	}
}

#endif /* OPENDS_TEST_CUDA_COMMON_H */
