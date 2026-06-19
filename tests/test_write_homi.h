/*
 * test_write_homi.h - Shared write-test logic for the aisio backend on a live
 * HOMI/qublk stack.
 *
 * Writes go through the kernel-mounted filesystem (pwrite), so these tests run
 * against a real, writable file on the mount. The flow exercises an allocating
 * write into a freshly created (empty) file, a P2P read-back of the written
 * data, and an in-place sub-page overwrite that must leave the rest of the
 * page intact. The submit_write callback selects sync vs async.
 */
#ifndef TEST_WRITE_HOMI_H_
#define TEST_WRITE_HOMI_H_

#include "opends.h"
#include "read_pattern.h"
#include "test_cuda_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define WRITE_TAIL_KEEP 100
#define WRITE_PARTIAL_LEN (PAGE - WRITE_TAIL_KEEP)

struct write_homi_env {
	opends_handle_t fh;
	opends_stream_t stream; /* unused by sync submit. */
	ssize_t (*submit_write)(struct write_homi_env *e, void *gpu,
	                        size_t size, off_t foff);
	const char *mode_label;
};

static inline unsigned char
write_pattern_byte(size_t i)
{
	return (unsigned char)((i * 7 + 0x5a) & 0xff);
}

/* Run against a handle whose file was created empty (O_CREAT|O_TRUNC). */
static int
run_write_homi_tests(struct write_homi_env *e)
{
	int failed = 0;
	void *gpu = opends_alloc(FILE_SIZE);
	unsigned char *host = malloc(FILE_SIZE);
	unsigned char *exp = malloc(FILE_SIZE);
	if (!gpu || !host || !exp) {
		fprintf(stderr, "[%s] allocation failed\n", e->mode_label);
		failed++;
		goto out;
	}

	/* 1. Allocating write of the whole file from empty. */
	for (size_t i = 0; i < FILE_SIZE; i++)
		exp[i] = write_pattern_byte(i);
	cuda_buf_from_host(gpu, exp, FILE_SIZE);
	ssize_t w = e->submit_write(e, gpu, FILE_SIZE, 0);
	if (w != (ssize_t)FILE_SIZE) {
		fprintf(stderr, "[%s] alloc write: %zd\n", e->mode_label, w);
		failed++;
		goto out;
	}

	/* 2. P2P read-back of the just-written, fsync'd data. */
	cuda_buf_zero(gpu, FILE_SIZE);
	ssize_t r = opends_read(e->fh, gpu, FILE_SIZE, 0, 0);
	cuda_buf_to_host(host, gpu, FILE_SIZE);
	if (r != (ssize_t)FILE_SIZE || memcmp(host, exp, FILE_SIZE)) {
		fprintf(stderr, "[%s] alloc verify failed (r=%zd)\n",
		        e->mode_label, r);
		failed++;
	} else {
		fprintf(stderr, "[%s] alloc write + read-back ok\n",
		        e->mode_label);
	}

	/* 3. In-place sub-page overwrite; the page tail must survive. */
	memset(exp, 0xBB, WRITE_PARTIAL_LEN);
	cuda_buf_from_host(gpu, exp, WRITE_PARTIAL_LEN);
	w = e->submit_write(e, gpu, WRITE_PARTIAL_LEN, 0);
	if (w != (ssize_t)WRITE_PARTIAL_LEN) {
		fprintf(stderr, "[%s] overwrite: %zd\n", e->mode_label, w);
		failed++;
		goto out;
	}

	for (size_t i = 0; i < WRITE_PARTIAL_LEN; i++)
		exp[i] = 0xBB;
	for (size_t i = WRITE_PARTIAL_LEN; i < PAGE; i++)
		exp[i] = write_pattern_byte(i);
	cuda_buf_zero(gpu, PAGE);
	r = opends_read(e->fh, gpu, PAGE, 0, 0);
	cuda_buf_to_host(host, gpu, PAGE);
	if (r != (ssize_t)PAGE || memcmp(host, exp, PAGE)) {
		fprintf(stderr, "[%s] overwrite tail-preserve verify failed\n",
		        e->mode_label);
		failed++;
	} else {
		fprintf(stderr, "[%s] in-place overwrite ok\n", e->mode_label);
	}

out:
	free(exp);
	free(host);
	if (gpu)
		opends_free(gpu);
	return failed;
}

#endif /* TEST_WRITE_HOMI_H_ */
