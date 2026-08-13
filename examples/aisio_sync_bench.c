/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * aisio_sync_bench - Thread-count sweep for the synchronous opends_read
 * path, the counterpart of aisio_ioq_bench for the thread-pool model:
 * N threads each keep one blocking read in flight, which is how the
 * NIXL plugin drives the sync API today.
 *
 * Threads read disjoint interleaved stripes of the file (thread t reads
 * blocks t, t+N, t+2N, ..., wrapping), each into its own device buffer.
 *
 * Usage: aisio_sync_bench <file-on-mount> <block_bytes> <threads> <total_mib>
 *
 * Requires the HOMI/qublk stack up and OPENDS_HOMI_DEV /
 * OPENDS_HOMI_SOCKET / OPENDS_HOMI_MNT exported, like the aisio tests.
 */
#define _GNU_SOURCE

#include "test_aisio_homi.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_THREADS 64

struct sync_worker {
	pthread_t tid;
	CUcontext cuctx;
	opends_handle_t fh;
	void *buf;
	size_t block;
	uint64_t span;
	uint64_t start_off;
	uint64_t stride;
	uint64_t nops;
	uint64_t errors;
};

static uint64_t
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void *
sync_worker_fn(void *arg)
{
	struct sync_worker *w = arg;
	uint64_t off = w->start_off;

	cuCtxSetCurrent(w->cuctx);

	for (uint64_t i = 0; i < w->nops; i++) {
		ssize_t n = opends_read(w->fh, w->buf, w->block, (off_t)off, 0);

		if (n != (ssize_t)w->block)
			w->errors++;
		off += w->stride;
		if (off + w->block > w->span)
			off = w->start_off;
	}
	return NULL;
}

int
main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr,
		        "usage: %s <file-on-mount> <block_bytes> <threads> "
		        "<total_mib>\n",
		        argv[0]);
		return 1;
	}

	size_t block = strtoull(argv[2], NULL, 0);
	unsigned nthreads = (unsigned)strtoul(argv[3], NULL, 0);
	uint64_t total = strtoull(argv[4], NULL, 0) << 20;

	if (!block || !nthreads || nthreads > MAX_THREADS || !total) {
		fprintf(stderr, "bad block/threads/total\n");
		return 1;
	}

	struct aisio_homi a;
	if (aisio_homi_setup(argv[1], &a) < 0)
		return 1;

	struct stat st;
	if (fstat(a.fd, &st) < 0 ||
	    (uint64_t)st.st_size < block * nthreads) {
		fprintf(stderr, "file smaller than one block per thread\n");
		aisio_homi_teardown(&a);
		return 1;
	}
	uint64_t span = ((uint64_t)st.st_size / block) * block;

	int rc = 1;
	struct sync_worker *ws = calloc(nthreads, sizeof(*ws));
	if (!ws)
		goto out;

	uint64_t nops = total / block;

	for (unsigned t = 0; t < nthreads; t++) {
		ws[t].cuctx = a.cuctx;
		ws[t].fh = a.fh;
		ws[t].block = block;
		ws[t].span = span;
		ws[t].start_off = (uint64_t)t * block;
		ws[t].stride = (uint64_t)nthreads * block;
		ws[t].nops = nops / nthreads + (t < nops % nthreads);
		ws[t].buf = opends_alloc(block);
		if (!ws[t].buf) {
			fprintf(stderr, "opends_alloc(%zu) failed at %u\n",
			        block, t);
			goto out;
		}
	}

	uint64_t t0 = now_ns();

	for (unsigned t = 0; t < nthreads; t++)
		pthread_create(&ws[t].tid, NULL, sync_worker_fn, &ws[t]);
	for (unsigned t = 0; t < nthreads; t++)
		pthread_join(ws[t].tid, NULL);

	uint64_t dt = now_ns() - t0;

	uint64_t done = 0;
	uint64_t errors = 0;
	for (unsigned t = 0; t < nthreads; t++) {
		done += ws[t].nops;
		errors += ws[t].errors;
	}

	double secs = (double)dt / 1e9;
	double gbs = (double)(done * block) / 1e9 / secs;
	double iops = (double)done / secs;
	double lat_us = (double)dt / 1e3 / (double)done * (double)nthreads;

	printf("block=%zu threads=%u ops=%" PRIu64 " errors=%" PRIu64
	       " time=%.3fs %.3f GB/s %.0f IOPS avg_lat=%.1fus\n",
	       block, nthreads, done, errors, secs, gbs, iops, lat_us);

	rc = errors ? 1 : 0;
out:
	if (ws)
		for (unsigned t = 0; t < nthreads; t++)
			if (ws[t].buf)
				opends_free(ws[t].buf);
	free(ws);
	aisio_homi_teardown(&a);
	return rc;
}
