#define _GNU_SOURCE

#include "test_cuda_common.h"
#include "test_sync_read.h"

#include <cuda.h>

int
main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!getenv("OPENDS_EXTENT_CACHE")) {
		fprintf(stderr, "OPENDS_EXTENT_CACHE must be set\n");
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
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, -1);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		ds_file_driver_close();
		return 1;
	}

	struct test_env env = {
		.fh = fh,
		.buf_to_host = cuda_buf_to_host,
		.buf_zero = cuda_buf_zero,
		.check_buffer = cuda_check_buffer,
	};

	fprintf(stderr, "ds_file_read sync tests (aisio backend)\n");
	int failed = run_sync_read_tests(&env);

	ds_file_handle_deregister(fh);
	ds_file_driver_close();
	cuCtxDestroy(cuctx);

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
