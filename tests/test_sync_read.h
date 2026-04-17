/*
 * test_sync_read.h - Backend-agnostic unit tests for synchronous
 * ds_file_read.
 *
 * Include from a backend-specific source file that provides main()
 * and initializes a struct test_env with buffer access callbacks.
 *
 * The test creates a 16-page (65536-byte) file where each page has
 * a distinct deterministic pattern seeded by its page number. Expected
 * read results are computed in memory via expected_bytes(), so the
 * test binary never needs to pread the test file for verification.
 */
#ifndef TEST_SYNC_READ_H_
#define TEST_SYNC_READ_H_

#include "opends.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PAGE 4096
#define FILE_PAGES 16
#define FILE_SIZE ((size_t)(PAGE * FILE_PAGES))

struct test_env {
	ds_file_handle_t fh;
	size_t file_size;
	void *(*buf_to_host)(void *dst, const void *src, size_t n);
	void (*buf_zero)(void *buf, size_t n);
};

static void
fill_pattern(char *buf, size_t size, unsigned char seed)
{
	for (size_t i = 0; i < size; i++)
		buf[i] = (unsigned char)((i + seed) & 0xff);
}

/*
 * Write FILE_PAGES pages of deterministic pattern data to fd.
 * Each page is seeded by its page number so adjacent pages differ.
 */
static int
write_test_file(int fd)
{
	char page[PAGE];

	for (int p = 0; p < FILE_PAGES; p++) {
		fill_pattern(page, PAGE, (unsigned char)p);
		if (pwrite(fd, page, PAGE, (off_t)(p * PAGE)) != PAGE) {
			perror("pwrite test data");
			return 1;
		}
	}
	return 0;
}

/*
 * Fill dst with the bytes that a read at (offset, size) should return.
 * Mirrors fill_pattern/write_test_file: at absolute file offset X the
 * byte value is ((X % PAGE) + (X / PAGE)) & 0xff. Size is clamped to
 * FILE_SIZE. Returns the number of bytes written to dst.
 */
static ssize_t
expected_bytes(char *dst, off_t offset, size_t size)
{
	if (offset < 0 || (size_t)offset >= FILE_SIZE)
		return 0;

	size_t avail = FILE_SIZE - (size_t)offset;
	if (size > avail)
		size = avail;

	for (size_t i = 0; i < size; i++) {
		off_t abs = offset + (off_t)i;
		unsigned long page = (unsigned long)abs / PAGE;
		unsigned long off_in_page = (unsigned long)abs % PAGE;
		dst[i] = (unsigned char)((off_in_page + page) & 0xff);
	}
	return (ssize_t)size;
}

/*
 * Read via ds_file_read, copy result to host, compare with the
 * in-memory expected pattern. Returns 0 on match.
 */
static int
verify_read(struct test_env *env, void *buf, size_t alloc_size, size_t size,
            off_t file_offset, off_t buf_offset, const char *label)
{
	env->buf_zero(buf, alloc_size);

	ssize_t n = ds_file_read(env->fh, buf, size, file_offset, buf_offset);
	if (n < 0) {
		fprintf(stderr, "  %s: ds_file_read: %s\n", label,
		        ds_file_op_status_error((ds_file_op_error_t)(-n)));
		return 1;
	}

	char *host = malloc(alloc_size);
	char *expected = malloc(size);

	if (!host || !expected) {
		free(host);
		free(expected);
		fprintf(stderr, "  %s: malloc failed\n", label);
		return 1;
	}

	env->buf_to_host(host, buf, alloc_size);

	ssize_t expected_n = expected_bytes(expected, file_offset, size);
	if (n != expected_n) {
		fprintf(stderr, "  %s: got %zd bytes, expected %zd\n", label, n,
		        expected_n);
		free(host);
		free(expected);
		return 1;
	}

	if (n > 0 && memcmp(host + buf_offset, expected, (size_t)n) != 0) {
		fprintf(stderr, "  %s: data mismatch\n", label);
		free(host);
		free(expected);
		return 1;
	}

	free(host);
	free(expected);
	return 0;
}

/* --- Test cases ------------------------------------------------- */

/* Read a single 4096-byte block at offset 0. */
static int
test_read_single_block(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, PAGE, PAGE, 0, 0, "single_block");

	ds_file_free(buf);
	return rc;
}

/* Read 4 contiguous blocks (16384 bytes) from offset 0. */
static int
test_read_multi_block(struct test_env *env)
{
	size_t size = 4 * PAGE;
	void *buf = ds_file_alloc(size);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, size, size, 0, 0, "multi_block");

	ds_file_free(buf);
	return rc;
}

/* Read one block from a non-zero aligned offset (page 2). */
static int
test_read_at_offset(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, PAGE, PAGE, 2 * PAGE, 0, "at_offset");

	ds_file_free(buf);
	return rc;
}

/* Read 2 blocks starting from page 1. */
static int
test_read_multi_at_offset(struct test_env *env)
{
	size_t size = 2 * PAGE;
	void *buf = ds_file_alloc(size);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, size, size, PAGE, 0,
	                     "multi_at_offset");

	ds_file_free(buf);
	return rc;
}

/* Read the entire file in one call. */
static int
test_read_full_file(struct test_env *env)
{
	void *buf = ds_file_alloc(FILE_SIZE);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, FILE_SIZE, FILE_SIZE, 0, 0,
	                     "full_file");

	ds_file_free(buf);
	return rc;
}

/* Read the last block of the file. */
static int
test_read_last_block(struct test_env *env)
{
	off_t off = (off_t)((FILE_PAGES - 1) * PAGE);
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, PAGE, PAGE, off, 0, "last_block");

	ds_file_free(buf);
	return rc;
}

/*
 * Request more bytes than remain at EOF. One page remains; request
 * two. Verify the return count and content of the short read.
 */
static int
test_read_short_at_eof(struct test_env *env)
{
	size_t req = 2 * PAGE;
	off_t off = (off_t)((FILE_PAGES - 1) * PAGE);
	void *buf = ds_file_alloc(req);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, req, req, off, 0, "short_at_eof");

	ds_file_free(buf);
	return rc;
}

/* Read at exact end of file; expect 0 bytes returned. */
static int
test_read_at_eof(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	env->buf_zero(buf, PAGE);
	ssize_t n = ds_file_read(env->fh, buf, PAGE, (off_t)FILE_SIZE, 0);
	if (n < 0) {
		fprintf(stderr, "  at_eof: %s\n",
		        ds_file_op_status_error((ds_file_op_error_t)(-n)));
		ds_file_free(buf);
		return 1;
	}

	ds_file_free(buf);
	if (n != 0) {
		fprintf(stderr, "  at_eof: got %zd bytes, expected 0\n", n);
		return 1;
	}
	return 0;
}

/* File offset past end of file; expect 0 bytes returned. */
static int
test_read_beyond_eof(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	env->buf_zero(buf, PAGE);
	ssize_t n = ds_file_read(env->fh, buf, PAGE,
	                         (off_t)(FILE_SIZE + PAGE), 0);
	if (n < 0) {
		fprintf(stderr, "  beyond_eof: %s\n",
		        ds_file_op_status_error((ds_file_op_error_t)(-n)));
		ds_file_free(buf);
		return 1;
	}

	ds_file_free(buf);
	if (n != 0) {
		fprintf(stderr, "  beyond_eof: got %zd bytes, expected 0\n", n);
		return 1;
	}
	return 0;
}

/* Read zero bytes; expect 0 returned. */
static int
test_read_zero_size(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	ssize_t n = ds_file_read(env->fh, buf, 0, 0, 0);

	ds_file_free(buf);
	if (n != 0) {
		fprintf(stderr, "  zero_size: got %zd, expected 0\n", n);
		return 1;
	}
	return 0;
}

/* Read with non-zero buf_offset; verify data lands correctly. */
static int
test_read_buf_offset(struct test_env *env)
{
	size_t alloc = 2 * PAGE;
	off_t boff = PAGE;
	void *buf = ds_file_alloc(alloc);
	if (!buf)
		return 1;

	int rc = verify_read(env, buf, alloc, PAGE, 0, boff, "buf_offset");

	ds_file_free(buf);
	return rc;
}

/*
 * Verify that a read with buf_offset modifies only the target
 * region. The prefix and suffix of the buffer must remain zero.
 */
static int
test_read_buf_offset_boundaries(struct test_env *env)
{
	size_t alloc = 3 * PAGE;
	off_t boff = PAGE;
	void *buf = ds_file_alloc(alloc);
	if (!buf)
		return 1;

	env->buf_zero(buf, alloc);

	ssize_t n = ds_file_read(env->fh, buf, PAGE, 0, boff);
	if (n != (ssize_t)PAGE) {
		fprintf(stderr, "  buf_offset_bounds: got %zd bytes\n", n);
		ds_file_free(buf);
		return 1;
	}

	char *host = malloc(alloc);
	if (!host) {
		ds_file_free(buf);
		return 1;
	}
	env->buf_to_host(host, buf, alloc);

	char *zeroes = calloc(1, PAGE);
	int rc = 0;

	if (memcmp(host, zeroes, PAGE) != 0) {
		fprintf(stderr, "  buf_offset_bounds: prefix modified\n");
		rc = 1;
	}

	if (memcmp(host + 2 * PAGE, zeroes, PAGE) != 0) {
		fprintf(stderr, "  buf_offset_bounds: suffix modified\n");
		rc = 1;
	}

	free(zeroes);
	free(host);
	ds_file_free(buf);
	return rc;
}

/* Read each quarter of the file independently, verify all. */
static int
test_read_sequential_regions(struct test_env *env)
{
	size_t quarter = FILE_SIZE / 4;
	void *buf = ds_file_alloc(quarter);
	if (!buf)
		return 1;

	int rc = 0;
	for (int i = 0; i < 4; i++) {
		char label[64];
		snprintf(label, sizeof(label), "sequential[%d]", i);
		rc = verify_read(env, buf, quarter, quarter,
		                 (off_t)(i * (off_t)quarter), 0, label);
		if (rc)
			break;
	}

	ds_file_free(buf);
	return rc;
}

/*
 * Read two overlapping regions (pages 0-1 and pages 1-2) and verify
 * the shared page is identical in both reads.
 */
static int
test_read_overlapping_regions(struct test_env *env)
{
	size_t size = 2 * PAGE;
	void *buf = ds_file_alloc(size);
	if (!buf)
		return 1;

	char *host_a = malloc(size);
	char *host_b = malloc(size);
	if (!host_a || !host_b) {
		free(host_a);
		free(host_b);
		ds_file_free(buf);
		return 1;
	}

	/* Read pages 0-1. */
	env->buf_zero(buf, size);
	ssize_t n1 = ds_file_read(env->fh, buf, size, 0, 0);
	if (n1 != (ssize_t)size) {
		fprintf(stderr, "  overlap: read A got %zd\n", n1);
		free(host_a);
		free(host_b);
		ds_file_free(buf);
		return 1;
	}
	env->buf_to_host(host_a, buf, size);

	/* Read pages 1-2. */
	env->buf_zero(buf, size);
	ssize_t n2 = ds_file_read(env->fh, buf, size, PAGE, 0);
	if (n2 != (ssize_t)size) {
		fprintf(stderr, "  overlap: read B got %zd\n", n2);
		free(host_a);
		free(host_b);
		ds_file_free(buf);
		return 1;
	}
	env->buf_to_host(host_b, buf, size);

	/* Page 1 appears at offset PAGE in read A and offset 0 in read B. */
	int rc = 0;
	if (memcmp(host_a + PAGE, host_b, PAGE) != 0) {
		fprintf(stderr, "  overlap: shared region mismatch\n");
		rc = 1;
	}

	free(host_a);
	free(host_b);
	ds_file_free(buf);
	return rc;
}

/* Read the same block twice; both reads must return identical data. */
static int
test_read_repeated(struct test_env *env)
{
	void *buf = ds_file_alloc(PAGE);
	if (!buf)
		return 1;

	char *host_a = malloc(PAGE);
	char *host_b = malloc(PAGE);
	if (!host_a || !host_b) {
		free(host_a);
		free(host_b);
		ds_file_free(buf);
		return 1;
	}

	env->buf_zero(buf, PAGE);
	ssize_t n1 = ds_file_read(env->fh, buf, PAGE, 0, 0);
	env->buf_to_host(host_a, buf, PAGE);

	env->buf_zero(buf, PAGE);
	ssize_t n2 = ds_file_read(env->fh, buf, PAGE, 0, 0);
	env->buf_to_host(host_b, buf, PAGE);

	int rc = 0;
	if (n1 != n2) {
		fprintf(stderr, "  repeated: byte counts differ (%zd vs %zd)\n",
		        n1, n2);
		rc = 1;
	} else if (n1 > 0 && memcmp(host_a, host_b, (size_t)n1) != 0) {
		fprintf(stderr, "  repeated: data mismatch\n");
		rc = 1;
	}

	free(host_a);
	free(host_b);
	ds_file_free(buf);
	return rc;
}

/*
 * Sweep a table of (offset, size) pairs covering misaligned starts,
 * mid-file reads that span pages, and a short read that extends past
 * EOF. Each pair is verified against the in-memory pattern oracle.
 */
static int
test_read_offset_size_sweep(struct test_env *env)
{
	static const struct {
		off_t offset;
		size_t size;
	} cases[] = {
		{    0,  4096},
		{    0, 65536},
		{ 4096,  4096},
		{ 8192,  4096},
		{    0, 32768},
		{ 4096, 65536},
		{    0, 16384},
		{ 8192, 32768},
	};

	size_t max_size = 0;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		if (cases[i].size > max_size)
			max_size = cases[i].size;
	}

	void *buf = ds_file_alloc(max_size);
	if (!buf)
		return 1;

	int rc = 0;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char label[64];
		snprintf(label, sizeof(label), "sweep[off=%ld,sz=%zu]",
		         (long)cases[i].offset, cases[i].size);
		rc = verify_read(env, buf, max_size, cases[i].size,
		                 cases[i].offset, 0, label);
		if (rc)
			break;
	}

	ds_file_free(buf);
	return rc;
}

/* --- Test runner ------------------------------------------------ */

struct test_entry {
	const char *name;
	int (*fn)(struct test_env *);
};

/* clang-format off */
static const struct test_entry sync_read_tests[] = {
	{"single_block",        test_read_single_block},
	{"multi_block",         test_read_multi_block},
	{"at_offset",           test_read_at_offset},
	{"multi_at_offset",     test_read_multi_at_offset},
	{"full_file",           test_read_full_file},
	{"last_block",          test_read_last_block},
	{"short_at_eof",        test_read_short_at_eof},
	{"at_eof",              test_read_at_eof},
	{"beyond_eof",          test_read_beyond_eof},
	{"zero_size",           test_read_zero_size},
	{"buf_offset",          test_read_buf_offset},
	{"buf_offset_bounds",   test_read_buf_offset_boundaries},
	{"sequential_regions",  test_read_sequential_regions},
	{"overlapping_regions", test_read_overlapping_regions},
	{"repeated",            test_read_repeated},
	{"offset_size_sweep",   test_read_offset_size_sweep},
};
/* clang-format on */

#define NSYNC_READ_TESTS (sizeof(sync_read_tests) / sizeof(sync_read_tests[0]))

static int
run_sync_read_tests(struct test_env *env)
{
	int failed = 0;

	for (size_t i = 0; i < NSYNC_READ_TESTS; i++) {
		int rc = sync_read_tests[i].fn(env);
		fprintf(stderr, "  %-24s %s\n", sync_read_tests[i].name,
		        rc ? "FAIL" : "ok");
		failed += rc;
	}
	return failed;
}

#endif /* TEST_SYNC_READ_H_ */
