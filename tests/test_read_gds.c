#define _GNU_SOURCE

#include "test_read.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <fcntl.h>

static void *
gds_buf_to_host(void *dst, const void *src, size_t n)
{
	cudaMemcpy(dst, src, n, cudaMemcpyDeviceToHost);
	return dst;
}

static void
gds_buf_zero(void *buf, size_t n)
{
	cudaMemset(buf, 0, n);
	cudaDeviceSynchronize();
}

int
main(int argc, char **argv)
{
	const char *dir = ".";
	if (argc > 1)
		dir = argv[1];

	char path[512];
	snprintf(path, sizeof(path), "%s/test_read_sync_XXXXXX", dir);
	int setup_fd = mkstemp(path);
	if (setup_fd < 0) {
		perror("mkstemp");
		return 1;
	}

	if (write_test_file(setup_fd)) {
		unlink(path);
		return 1;
	}
	fsync(setup_fd);
	close(setup_fd);

	cuInit(0);
	CUdevice cudev;
	CUcontext cuctx;
	cuDeviceGet(&cudev, 0);
	cuCtxCreate(&cuctx, 0, cudev);

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		unlink(path);
		return 1;
	}

	int fd = open(path, O_RDWR | O_DIRECT);
	if (fd < 0) {
		perror("open O_DIRECT");
		unlink(path);
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, fd);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		unlink(path);
		return 1;
	}

	struct test_env env = {
		.fh = fh,
		.file_size = FILE_SIZE,
		.buf_to_host = gds_buf_to_host,
		.buf_zero = gds_buf_zero,
	};

	fprintf(stderr, "ds_file_read sync tests (gds backend)\n");
	int failed = run_read_tests(&env);

	ds_file_handle_deregister(fh);
	close(fd);
	ds_file_driver_close();
	unlink(path);
	cuCtxDestroy(cuctx);

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
