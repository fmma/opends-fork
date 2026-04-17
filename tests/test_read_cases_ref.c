#define _GNU_SOURCE

#include "opends.h"
#include "read_cases.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <input_file>\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	size_t max_size = read_max_size();

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open: %s\n",
		        ds_file_op_status_error(err.err));
		return 1;
	}

	int fd = open(path, O_RDONLY | O_DIRECT);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	int ref_fd = open(path, O_RDONLY);
	if (ref_fd < 0) {
		perror("open (reference)");
		return 1;
	}

	ds_file_handle_t fh;
	err = ds_file_handle_register(&fh, fd);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		return 1;
	}

	void *buf = ds_file_alloc(max_size);
	void *expected = malloc(max_size);
	if (!buf || !expected) {
		fprintf(stderr, "alloc failed\n");
		return 1;
	}

	int rc = 0;
	for (size_t i = 0; i < NCASES; i++) {
		size_t sz = cases[i].size;
		off_t off = cases[i].offset;

		memset(buf, 0, sz);

		ssize_t n = ds_file_read(fh, buf, sz, off, 0);
		if (n < 0) {
			fprintf(stderr, "case %zu (off=%ld, sz=%zu): %s\n",
			        i, (long)off, sz,
			        ds_file_op_status_error(
			                (ds_file_op_error_t)-n));
			rc = 1;
			break;
		}

		ssize_t ref_n = pread(ref_fd, expected, sz, off);
		if (ref_n != n) {
			fprintf(stderr,
			        "case %zu: got %zd bytes, expected %zd\n",
			        i, n, ref_n);
			rc = 1;
			break;
		}

		if (memcmp(buf, expected, (size_t)n) != 0) {
			fprintf(stderr,
			        "case %zu (off=%ld, sz=%zu): data mismatch\n",
			        i, (long)off, sz);
			rc = 1;
			break;
		}

		fprintf(stderr, "case %zu (off=%ld, sz=%zu): ok\n",
		        i, (long)off, sz);
	}

	free(expected);
	ds_file_free(buf);
	ds_file_handle_deregister(fh);
	close(ref_fd);
	close(fd);
	ds_file_driver_close();

	if (!rc)
		fprintf(stderr, "all ok\n");

	return rc;
}
