#define _GNU_SOURCE

#include "test_aisio_homi.h"
#include "test_cuda_common.h"
#include "test_sync_read.h"

#include <cuda.h>

#include <stdio.h>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <pattern-file-on-mount>\n", argv[0]);
		return 1;
	}

	struct aisio_homi a;
	if (aisio_homi_setup(argv[1], &a) < 0)
		return 1;

	fprintf(stderr, "ds_file_read sync tests (aisio backend, HOMI)\n");

	struct test_env env_alloc = {
	        .fh = a.fh,
	        .buf_to_host = cuda_buf_to_host,
	        .buf_zero = cuda_buf_zero,
	        .check_buffer = cuda_check_buffer,
	        .buf_acquire = cuda_alloc_acquire,
	        .buf_release = cuda_alloc_release,
	        .mode_label = "alloc",
	};
	int failed = run_sync_read_tests(&env_alloc);

	struct test_env env_register = {
	        .fh = a.fh,
	        .buf_to_host = cuda_buf_to_host,
	        .buf_zero = cuda_buf_zero,
	        .check_buffer = cuda_check_buffer,
	        .buf_acquire = cuda_register_acquire,
	        .buf_release = cuda_register_release,
	        .mode_label = "register",
	};
	failed += run_sync_read_tests(&env_register);

	aisio_homi_teardown(&a);

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
