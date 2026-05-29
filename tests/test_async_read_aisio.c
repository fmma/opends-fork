#define _GNU_SOURCE

#include "fs_mock.h"
#include "test_async_read.h"
#include "test_cuda_common.h"
#include "test_extents_io.h"

#include <cuda.h>

#include <stdlib.h>

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
		fprintf(stderr, "fs_mock_init(%s): %s\n", uri, strerror(-rc));
		free(extents);
		return 1;
	}

	int mock_fh = fs_mock_register(extents, n_extents, 0, NULL);
	if (mock_fh < 0) {
		fprintf(stderr, "fs_mock_register: %s\n", strerror(-mock_fh));
		free(extents);
		fs_mock_reset();
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

	CUstream main_stream;
	if (cuStreamCreate(&main_stream, CU_STREAM_NON_BLOCKING) !=
	    CUDA_SUCCESS) {
		fprintf(stderr, "cuStreamCreate(main) failed\n");
		ds_file_handle_deregister(fh);
		ds_file_driver_close();
		cuCtxDestroy(cuctx);
		fs_mock_reset();
		return 1;
	}

	if (ds_file_stream_register(main_stream, 0).err != DS_FILE_SUCCESS) {
		fprintf(stderr, "ds_file_stream_register(main) failed\n");
		cuStreamDestroy(main_stream);
		ds_file_handle_deregister(fh);
		ds_file_driver_close();
		cuCtxDestroy(cuctx);
		fs_mock_reset();
		return 1;
	}

	CUstream extras[ASYNC_TEST_MAX_STREAMS];
	int extra_count = ASYNC_TEST_MAX_STREAMS;
	for (int i = 0; i < extra_count; i++) {
		if (cuStreamCreate(&extras[i], CU_STREAM_NON_BLOCKING) !=
		    CUDA_SUCCESS) {
			fprintf(stderr, "cuStreamCreate(extra[%d]) failed\n", i);
			extra_count = i;
			break;
		}
		if (ds_file_stream_register(extras[i], 0).err !=
		    DS_FILE_SUCCESS) {
			fprintf(stderr, "ds_file_stream_register(extra[%d]) failed\n", i);
			cuStreamDestroy(extras[i]);
			extra_count = i;
			break;
		}
	}

	fprintf(stderr, "ds_file_read_async tests (aisio backend)\n");

	struct async_test_env env_alloc = {
		.fh = fh,
		.stream = main_stream,
		.extra_stream_count = extra_count,
		.buf_to_host = cuda_buf_to_host,
		.buf_zero = cuda_buf_zero,
		.check_buffer = cuda_check_buffer,
		.buf_acquire = cuda_alloc_acquire,
		.buf_release = cuda_alloc_release,
		.mode_label = "alloc",
	};
	for (int i = 0; i < extra_count; i++)
		env_alloc.extra_streams[i] = extras[i];
	int failed = run_async_read_tests(&env_alloc);
	if (failed) {
		/* A failed alloc-mode test left a stream stuck on
		 * cuStreamWaitValue32; running register mode on the same
		 * streams would hang too. Bail out now. */
		fprintf(stderr, "%d test(s) failed\n", failed);
		fflush(NULL);
		_exit(1);
	}

	struct async_test_env env_register = {
		.fh = fh,
		.stream = main_stream,
		.extra_stream_count = extra_count,
		.buf_to_host = cuda_buf_to_host,
		.buf_zero = cuda_buf_zero,
		.check_buffer = cuda_check_buffer,
		.buf_acquire = cuda_register_acquire,
		.buf_release = cuda_register_release,
		.mode_label = "register",
	};
	for (int i = 0; i < extra_count; i++)
		env_register.extra_streams[i] = extras[i];
	failed += run_async_read_tests(&env_register);

	if (failed) {
		/* A timed-out test leaves streams stuck on
		 * cuStreamWaitValue32 and the I/O thread polling a never-
		 * ready event. Orderly teardown (ds_file_driver_close,
		 * cuCtxDestroy) would block forever waiting for that work
		 * to drain, so report and bail out. */
		fprintf(stderr, "%d test(s) failed\n", failed);
		fflush(NULL);
		_exit(1);
	}

	free(extents);

	for (int i = 0; i < extra_count; i++)
		cuStreamDestroy(extras[i]);
	cuStreamDestroy(main_stream);

	ds_file_handle_deregister(fh);
	ds_file_driver_close();
	cuCtxDestroy(cuctx);
	fs_mock_reset();

	fprintf(stderr, "all ok\n");
	return 0;
}
