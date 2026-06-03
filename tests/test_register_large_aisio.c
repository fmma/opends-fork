/*
 * Regression test: ds_file_buf_register on an 8 GiB cudaMalloc'd
 * buffer. Mirrors the bench-time flow where fil allocates a single
 * device buffer sized to the dataset's max file (8 GiB for the
 * filesize8gib dataset) and hands it to ds_file_buf_register.
 *
 * Catches regressions in the cudaMalloc → xnvme_mem_map →
 * cuMemGetHandleForAddressRange path at scale (LUT capacity, BAR1
 * contiguity, alloc_granularity handling) that small-buffer tests
 * don't exercise.
 */

#define _GNU_SOURCE

#include "opends.h"
#include "test_aisio_homi.h"

#include <cuda_runtime.h>

#include <stdio.h>

#define BUF_SIZE ((size_t)8 * 1024 * 1024 * 1024)

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <file-on-mount>\n", argv[0]);
		return 1;
	}

	struct aisio_homi a;
	if (aisio_homi_setup(argv[1], &a) < 0)
		return 1;

	void *buf = NULL;
	cudaError_t cerr = cudaMalloc(&buf, BUF_SIZE);
	if (cerr != cudaSuccess) {
		fprintf(stderr, "cudaMalloc(%zu): %s\n", BUF_SIZE,
		        cudaGetErrorString(cerr));
		aisio_homi_teardown(&a);
		return 1;
	}

	ds_file_error_t err = ds_file_buf_register(buf, BUF_SIZE, 0);
	int failed = 0;
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "buf_register(%zu): %s\n", BUF_SIZE,
		        ds_file_op_status_error(err.err));
		failed = 1;
	} else {
		ds_file_buf_deregister(buf);
	}

	cudaFree(buf);
	aisio_homi_teardown(&a);

	if (failed)
		fprintf(stderr, "register_large_aisio FAIL\n");
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
