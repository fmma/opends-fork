#define _GNU_SOURCE

#include "opends.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

static int
check(ds_file_error_t err, const char *label)
{
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "%s: %s\n", label,
		        ds_file_op_status_error(err.err));
		return 1;
	}
	return 0;
}

static void
fill_pattern(char *buf, size_t size, unsigned char seed)
{
	for (size_t i = 0; i < size; i++)
		buf[i] = (i + seed) & 0xff;
}

static int
test_sync_read_write(ds_file_handle_t fh, char *wbuf, char *rbuf)
{
	fill_pattern(wbuf, BUF_SIZE, 0);

	ssize_t w = ds_file_write(fh, wbuf, BUF_SIZE, 0, 0);
	if (w != BUF_SIZE) {
		fprintf(stderr, "sync write: %zd, expected %d\n", w, BUF_SIZE);
		return 1;
	}

	memset(rbuf, 0, BUF_SIZE);
	ssize_t n = ds_file_read(fh, rbuf, BUF_SIZE, 0, 0);
	if (n != BUF_SIZE) {
		fprintf(stderr, "sync read: %zd, expected %d\n", n, BUF_SIZE);
		return 1;
	}

	if (memcmp(wbuf, rbuf, BUF_SIZE) != 0) {
		fprintf(stderr, "sync read/write mismatch\n");
		return 1;
	}

	return 0;
}

static int
test_buf_offset(ds_file_handle_t fh, char *wbuf)
{
	fill_pattern(wbuf, BUF_SIZE, 0);

	ssize_t w = ds_file_write(fh, wbuf, BUF_SIZE, 4096, 0);
	if (w != BUF_SIZE) {
		fprintf(stderr, "buf_offset write: %zd, expected %d\n", w,
		        BUF_SIZE);
		return 1;
	}

	size_t big_size = BUF_SIZE + 512;
	char *bigbuf = ds_file_alloc(big_size);
	if (!bigbuf) {
		fprintf(stderr, "ds_file_alloc failed\n");
		return 1;
	}
	memset(bigbuf, 0, big_size);

	ssize_t n = ds_file_read(fh, bigbuf, BUF_SIZE, 4096, 512);
	if (n != BUF_SIZE) {
		fprintf(stderr, "buf_offset read: %zd, expected %d\n", n,
		        BUF_SIZE);
		ds_file_free(bigbuf);
		return 1;
	}

	int rc = memcmp(bigbuf + 512, wbuf, BUF_SIZE) != 0;
	if (rc)
		fprintf(stderr, "buf_offset read/write mismatch\n");

	ds_file_free(bigbuf);
	return rc;
}

static int
test_driver_properties(void)
{
	unsigned major, minor, patch;
	if (check(ds_file_get_version(&major, &minor, &patch), "get_version"))
		return 1;
	if (major != 0 || minor != 1 || patch != 0) {
		fprintf(stderr, "version: %u.%u.%u, expected 0.1.0\n", major,
		        minor, patch);
		return 1;
	}

	if (ds_file_use_count() != 1) {
		fprintf(stderr, "use_count: %ld, expected 1\n",
		        ds_file_use_count());
		return 1;
	}

	return 0;
}

static int
test_batch_io(ds_file_handle_t fh, char *wbuf, char *rbuf)
{
	ds_file_batch_handle_t batch;
	if (check(ds_file_batch_io_setup(&batch, 4), "batch_io_setup"))
		return 1;

	fill_pattern(wbuf, BUF_SIZE, 0x42);

	ds_file_io_params_t params[2] = {
	        {
	                .mode = DS_FILE_BATCH,
	                .u.batch = {wbuf, 8192, 0, BUF_SIZE},
	                .fh = fh,
	                .opcode = DS_FILE_WRITE,
	                .cookie = (void *)1,
	        },
	        {
	                .mode = DS_FILE_BATCH,
	                .u.batch = {rbuf, 8192, 0, BUF_SIZE},
	                .fh = fh,
	                .opcode = DS_FILE_READ,
	                .cookie = (void *)2,
	        },
	};

	if (check(ds_file_batch_io_submit(batch, 2, params, 0),
	          "batch_io_submit")) {
		ds_file_batch_io_destroy(batch);
		return 1;
	}

	ds_file_io_events_t events[2];
	unsigned nr_events = 2;
	if (check(ds_file_batch_io_get_status(batch, 2, &nr_events, events,
	                                      NULL),
	          "batch_io_get_status")) {
		ds_file_batch_io_destroy(batch);
		return 1;
	}

	if (nr_events != 2) {
		fprintf(stderr, "batch events: %u, expected 2\n", nr_events);
		ds_file_batch_io_destroy(batch);
		return 1;
	}

	for (unsigned i = 0; i < nr_events; i++) {
		if (events[i].status != DS_FILE_COMPLETE ||
		    events[i].ret != BUF_SIZE) {
			fprintf(stderr, "batch event %u: status=%d ret=%zu\n",
			        i, events[i].status, events[i].ret);
			ds_file_batch_io_destroy(batch);
			return 1;
		}
	}

	ds_file_batch_io_destroy(batch);

	if (memcmp(wbuf, rbuf, BUF_SIZE) != 0) {
		fprintf(stderr, "batch read/write mismatch\n");
		return 1;
	}

	return 0;
}

static int
test_async_io(ds_file_handle_t fh, char *wbuf, char *rbuf)
{
	ds_stream_t stream = NULL;
	if (check(ds_file_stream_register(stream, 0), "stream_register"))
		return 1;

	fill_pattern(wbuf, BUF_SIZE, 0x99);

	size_t size = BUF_SIZE;
	off_t file_off = 12288;
	off_t buf_off = 0;
	ssize_t bytes = 0;

	if (check(ds_file_write_async(fh, wbuf, &size, &file_off, &buf_off,
	                              &bytes, stream),
	          "write_async"))
		return 1;
	if (bytes != BUF_SIZE) {
		fprintf(stderr, "write_async bytes: %zd, expected %d\n", bytes,
		        BUF_SIZE);
		return 1;
	}

	memset(rbuf, 0, BUF_SIZE);
	bytes = 0;

	if (check(ds_file_read_async(fh, rbuf, &size, &file_off, &buf_off,
	                             &bytes, stream),
	          "read_async"))
		return 1;
	if (bytes != BUF_SIZE) {
		fprintf(stderr, "read_async bytes: %zd, expected %d\n", bytes,
		        BUF_SIZE);
		return 1;
	}

	if (memcmp(wbuf, rbuf, BUF_SIZE) != 0) {
		fprintf(stderr, "async read/write mismatch\n");
		return 1;
	}

	if (check(ds_file_stream_deregister(stream), "stream_deregister"))
		return 1;
	return 0;
}

int
main(void)
{
	char path[] = "/tmp/smoke_ref_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		perror("mkstemp");
		return 1;
	}
	unlink(path);

	if (check(ds_file_driver_open(), "driver_open"))
		return 1;

	ds_file_handle_t fh;
	if (check(ds_file_handle_register(&fh, fd), "handle_register"))
		return 1;

	char *wbuf = ds_file_alloc(BUF_SIZE);
	char *rbuf = ds_file_alloc(BUF_SIZE);
	if (!wbuf || !rbuf) {
		fprintf(stderr, "ds_file_alloc failed\n");
		return 1;
	}

	if (test_sync_read_write(fh, wbuf, rbuf))
		return 1;
	if (test_buf_offset(fh, wbuf))
		return 1;
	if (test_driver_properties())
		return 1;
	if (test_batch_io(fh, wbuf, rbuf))
		return 1;
	if (test_async_io(fh, wbuf, rbuf))
		return 1;

	ds_file_free(rbuf);
	ds_file_free(wbuf);
	ds_file_handle_deregister(fh);
	close(fd);

	if (ds_file_use_count() != 0) {
		fprintf(stderr, "use_count after deregister: %ld, expected 0\n",
		        ds_file_use_count());
		return 1;
	}

	ds_file_driver_close();

	fprintf(stderr, "all ok\n");
	return 0;
}
