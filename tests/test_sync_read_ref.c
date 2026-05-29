#define _GNU_SOURCE

#include "test_sync_read.h"

#include <fcntl.h>
#include <stdlib.h>

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

static void *
ref_alloc_acquire(size_t size)
{
	return ds_file_alloc(size);
}

static void
ref_alloc_release(void *buf)
{
	ds_file_free(buf);
}

#define PAGE_ALIGN(x) (((x) + 4095) & ~((size_t)4095))

static void *
ref_register_acquire(size_t size)
{
	size_t aligned = PAGE_ALIGN(size);
	void *buf = aligned_alloc(4096, aligned);
	if (!buf)
		return NULL;
	ds_file_error_t err = ds_file_buf_register(buf, aligned, 0);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "  buf_register: %s\n",
		        ds_file_op_status_error(err.err));
		free(buf);
		return NULL;
	}
	return buf;
}

static void
ref_register_release(void *buf)
{
	if (!buf)
		return;
	ds_file_buf_deregister(buf);
	free(buf);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <path>\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		close(fd);
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

	fprintf(stderr, "ds_file_read sync tests (ref backend)\n");

	struct test_env env_alloc = {
	        .fh = fh,
	        .buf_to_host = ref_buf_to_host,
	        .buf_zero = ref_buf_zero,
	        .buf_acquire = ref_alloc_acquire,
	        .buf_release = ref_alloc_release,
	        .mode_label = "alloc",
	};
	int failed = run_sync_read_tests(&env_alloc);

	struct test_env env_register = {
	        .fh = fh,
	        .buf_to_host = ref_buf_to_host,
	        .buf_zero = ref_buf_zero,
	        .buf_acquire = ref_register_acquire,
	        .buf_release = ref_register_release,
	        .mode_label = "register",
	};
	failed += run_sync_read_tests(&env_register);

	ds_file_handle_deregister(fh);
	close(fd);
	ds_file_driver_close();

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");

	return failed ? 1 : 0;
}
