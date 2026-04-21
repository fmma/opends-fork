#define _GNU_SOURCE

#include "test_cuda_common.h"
#include "test_sync_read.h"

#include <cuda.h>

#include <fcntl.h>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return 1;
	}

	cuInit(0);
	CUdevice cudev;
	CUcontext cuctx;
	cuDeviceGet(&cudev, 0);
	cuCtxCreate(&cuctx, 0, cudev);

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		return 1;
	}

	int fd = open(argv[1], O_RDONLY | O_DIRECT);
	if (fd < 0) {
		perror("open O_DIRECT");
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, fd);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		close(fd);
		return 1;
	}

	fprintf(stderr, "ds_file_read sync tests (gds backend)\n");

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
	close(fd);
	ds_file_driver_close();
	cuCtxDestroy(cuctx);

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
