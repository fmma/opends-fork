/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * test_batch_read.h - Backend-agnostic batch read test.
 *
 * Include from a backend-specific source file that provides main().
 *
 * Submits one batch of concurrent reads at mixed offsets and sizes,
 * including sub-block sizes to cover per-op tail staging, reaps them
 * with a single get_status, and verifies each completion against the
 * in-memory pattern by cookie. A second get_status must deliver
 * nothing: completions are reported exactly once.
 *
 * Expects the 16-page pattern file written by test_sync_read_prep.
 */
#ifndef TEST_BATCH_READ_H_
#define TEST_BATCH_READ_H_

#include "opends.h"
#include "read_pattern.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct batch_read_env {
	opends_handle_t fh;
	void *(*buf_to_host)(void *dst, const void *src, size_t n);
	void *(*buf_acquire)(size_t size);
	void (*buf_release)(void *buf);
};

static const struct {
	off_t offset;
	size_t size;
} batch_read_cases[] = {
        {0, PAGE}, {PAGE, 2 * PAGE},       {4 * PAGE, 4 * PAGE},
        {0, 700},  {2 * PAGE, PAGE + 300}, {(FILE_PAGES - 1) * PAGE, PAGE},
};

#define BATCH_READ_NCASES                                                      \
	(sizeof(batch_read_cases) / sizeof(batch_read_cases[0]))

static int
batch_verify_event(struct batch_read_env *env, const opends_io_events_t *ev,
                   void **bufs, char *host)
{
	size_t ci = (size_t)(uintptr_t)ev->cookie;

	if (ci >= BATCH_READ_NCASES) {
		fprintf(stderr, "  bad cookie %zu\n", ci);
		return -1;
	}

	off_t offset = batch_read_cases[ci].offset;
	size_t size = batch_read_cases[ci].size;

	if (ev->status != OPENDS_COMPLETE || ev->ret != size) {
		fprintf(stderr,
		        "  case %zu (off=%ld, size=%zu): status=%d "
		        "ret=%zu\n",
		        ci, (long)offset, size, ev->status, ev->ret);
		return -1;
	}

	env->buf_to_host(host, bufs[ci], size);
	for (size_t b = 0; b < size; b++) {
		if ((unsigned char)host[b] != pattern_byte(offset + (off_t)b)) {
			fprintf(stderr,
			        "  mismatch at byte %zu (off=%ld, size=%zu)\n",
			        b, (long)offset, size);
			return -1;
		}
	}
	return 0;
}

static int
run_batch_read_test(struct batch_read_env *env)
{
	void *bufs[BATCH_READ_NCASES];
	opends_io_params_t params[BATCH_READ_NCASES];
	opends_io_events_t events[BATCH_READ_NCASES + 2];
	bool seen[BATCH_READ_NCASES];
	opends_batch_handle_t batch = NULL;
	int failures = 0;

	char *host = malloc(FILE_SIZE);
	if (!host) {
		fprintf(stderr, "  host buffer alloc failed\n");
		return 1;
	}

	memset(bufs, 0, sizeof(bufs));
	memset(seen, 0, sizeof(seen));
	for (size_t i = 0; i < BATCH_READ_NCASES; i++) {
		bufs[i] = env->buf_acquire(FILE_SIZE);
		if (!bufs[i]) {
			fprintf(stderr, "  buffer %zu alloc failed\n", i);
			failures = 1;
			goto out;
		}
		params[i] = (opends_io_params_t){
		        .mode = OPENDS_BATCH,
		        .u.batch = {bufs[i], batch_read_cases[i].offset, 0,
		                    batch_read_cases[i].size},
		        .fh = env->fh,
		        .opcode = OPENDS_READ,
		        .cookie = (void *)(uintptr_t)i,
		};
	}

	opends_error_t err = opends_batch_setup(&batch, BATCH_READ_NCASES + 2);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "  batch_setup: %s\n",
		        opends_op_status_error(err.err));
		failures = 1;
		goto out;
	}

	err = opends_batch_submit(batch, BATCH_READ_NCASES, params, 0);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "  batch_submit: %s\n",
		        opends_op_status_error(err.err));
		failures = 1;
		goto out;
	}

	unsigned nr = BATCH_READ_NCASES + 2;
	err = opends_batch_get_status(batch, BATCH_READ_NCASES, &nr, events,
	                              NULL);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "  batch_get_status: %s\n",
		        opends_op_status_error(err.err));
		failures = 1;
		goto out;
	}

	if (nr != BATCH_READ_NCASES) {
		fprintf(stderr, "  events: %u, expected %zu\n", nr,
		        BATCH_READ_NCASES);
		failures++;
	}

	for (unsigned i = 0; i < nr; i++) {
		size_t ci = (size_t)(uintptr_t)events[i].cookie;

		if (ci < BATCH_READ_NCASES && seen[ci]) {
			fprintf(stderr, "  duplicate event for case %zu\n", ci);
			failures++;
			continue;
		}
		if (batch_verify_event(env, &events[i], bufs, host) < 0)
			failures++;
		else
			seen[ci] = true;
	}

	/* Completions are delivered once; a second poll must be empty. */
	nr = BATCH_READ_NCASES + 2;
	err = opends_batch_get_status(batch, 0, &nr, events, NULL);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "  batch_get_status(repoll): %s\n",
		        opends_op_status_error(err.err));
		failures++;
	} else if (nr != 0) {
		fprintf(stderr, "  repoll events: %u, expected 0\n", nr);
		failures++;
	}

out:
	if (batch)
		opends_batch_destroy(batch);
	for (size_t i = 0; i < BATCH_READ_NCASES; i++)
		if (bufs[i])
			env->buf_release(bufs[i]);
	free(host);

	fprintf(stderr, "  %-24s %s\n", "batch_read", failures ? "FAIL" : "ok");
	return failures;
}

#endif /* TEST_BATCH_READ_H_ */
