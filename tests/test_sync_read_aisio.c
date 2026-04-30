#define _GNU_SOURCE

#include "fs_mock.h"
#include "test_cuda_common.h"
#include "test_sync_read.h"

#include <cuda.h>

int
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <extent_cache.bin> <pattern_path>\n",
		        argv[0]);
		return 1;
	}
	const char *cache_path = argv[1];
	const char *pattern_path = argv[2];

	int rc = fs_mock_init(cache_path);
	if (rc < 0) {
		fprintf(stderr, "fs_mock_init(%s): %s\n", cache_path,
		        strerror(-rc));
		return 1;
	}

	int mock_fh = fs_mock_open(pattern_path);
	if (mock_fh < 0) {
		fprintf(stderr, "fs_mock_open(%s): %s\n", pattern_path,
		        strerror(-mock_fh));
		fs_mock_reset();
		return 1;
	}

	/*
	 * xNVMe's upcie-cuda backend runs NVMe identify commands during
	 * device init and needs a current CUDA context for that.
	 */
	cuInit(0);
	CUdevice cudev;
	CUcontext cuctx;
	cuDeviceGet(&cudev, 0);
	cuCtxCreate(&cuctx, 0, cudev);

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

	fprintf(stderr, "ds_file_read sync tests (aisio backend)\n");

	struct test_env env_alloc = {
		.fh = fh,
		.buf_to_host = cuda_buf_to_host,
		.buf_zero = cuda_buf_zero,
		.check_buffer = cuda_check_buffer,
		.buf_acquire = cuda_alloc_acquire,
		.buf_release = cuda_alloc_release,
		.mode_label = "alloc",
	};
	int failed = run_sync_read_tests(&env_alloc);

	struct test_env env_register = {
		.fh = fh,
		.buf_to_host = cuda_buf_to_host,
		.buf_zero = cuda_buf_zero,
		.check_buffer = cuda_check_buffer,
		.buf_acquire = cuda_register_acquire,
		.buf_release = cuda_register_release,
		.mode_label = "register",
	};
	failed += run_sync_read_tests(&env_register);

	ds_file_handle_deregister(fh);
	ds_file_driver_close();
	cuCtxDestroy(cuctx);
	fs_mock_reset();

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
