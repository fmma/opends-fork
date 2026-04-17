#define _GNU_SOURCE

#include "test_read.h"

#include <fcntl.h>

static void *
ref_buf_to_host(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}

static void
ref_buf_zero(void *buf, size_t n)
{
	memset(buf, 0, n);
}

int
main(void)
{
	char path[] = "/tmp/test_read_sync_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		perror("mkstemp");
		return 1;
	}
	unlink(path);

	if (write_test_file(fd))
		return 1;

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, fd);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		return 1;
	}

	struct test_env env = {
		.fh = fh,
		.file_size = FILE_SIZE,
		.buf_to_host = ref_buf_to_host,
		.buf_zero = ref_buf_zero,
	};

	fprintf(stderr, "ds_file_read sync tests (ref backend)\n");
	int failed = run_read_tests(&env);

	ds_file_handle_deregister(fh);
	close(fd);
	ds_file_driver_close();

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
