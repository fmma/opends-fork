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

#include "fs_mock.h"
#include "opends.h"
#include "test_extents_io.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE ((size_t)8 * 1024 * 1024 * 1024)

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <extents_path>\n", argv[0]);
		return 1;
	}
	const char *extents_path = argv[1];

	char uri[64];
	struct homi_extent *extents = NULL;
	uint32_t n_extents = 0;
	int rc = test_extents_load(extents_path, uri, &extents, &n_extents);
	if (rc < 0) {
		fprintf(stderr, "test_extents_load(%s): %s\n", extents_path,
		        strerror(-rc));
		return 1;
	}

	rc = fs_mock_init(uri);
	if (rc < 0) {
		fprintf(stderr, "fs_mock_init(%s): %d\n", uri, rc);
		free(extents);
		return 1;
	}

	int mock_fh = fs_mock_register(extents, n_extents);
	free(extents);
	if (mock_fh < 0) {
		fprintf(stderr, "fs_mock_register: %d\n", mock_fh);
		fs_mock_reset();
		return 1;
	}

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		fs_mock_reset();
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, mock_fh);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		ds_file_driver_close();
		fs_mock_reset();
		return 1;
	}

	void *buf = NULL;
	cudaError_t cerr = cudaMalloc(&buf, BUF_SIZE);
	if (cerr != cudaSuccess) {
		fprintf(stderr, "cudaMalloc(%zu): %s\n", BUF_SIZE,
		        cudaGetErrorString(cerr));
		ds_file_handle_deregister(fh);
		ds_file_driver_close();
		fs_mock_reset();
		return 1;
	}

	err = ds_file_buf_register(buf, BUF_SIZE, 0);
	int failed = 0;
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "buf_register(%zu): %s\n", BUF_SIZE,
		        ds_file_op_status_error(err.err));
		failed = 1;
	} else {
		ds_file_buf_deregister(buf);
	}

	cudaFree(buf);
	ds_file_handle_deregister(fh);
	ds_file_driver_close();
	fs_mock_reset();

	if (failed)
		fprintf(stderr, "register_large_aisio FAIL\n");
	else
		fprintf(stderr, "all ok\n");

	return failed;
}
