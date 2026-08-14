/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * aisio_async_bench - Queue-depth sweep for the opends_async_* API.
 *
 * Reads a file on the HOMI mount sequentially (wrapping) into device
 * memory, keeping `qd` operations in flight per submitter thread by
 * awaiting the oldest future and resubmitting into its slot, each
 * thread striping a disjoint interleaved range. Reports aggregate
 * throughput and per-op latency. Compare qd=1 against the synchronous
 * opends_sync_read floor and higher depths against the device limit.
 *
 * Usage: aisio_async_bench <file-on-mount> <block_bytes> <qd> <total_mib>
 *                          [threads]
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

#define MAX_QD 1024
#define MAX_THREADS 16

struct async_worker {
	pthread_t tid;
	CUcontext cuctx;
	opends_handle_t fh;
	void **bufs;
	opends_async_future_t *futures;
	size_t block;
	unsigned qd;
	uint64_t span;
	uint64_t start_off;
	uint64_t stride;
	uint64_t nops;
	uint64_t done;
	uint64_t errors;
	int failed;
};

static uint64_t
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void *
async_worker_fn(void *arg)
{
	struct async_worker *w = arg;
	uint64_t submitted = 0;
	uint64_t off = w->start_off;
	opends_error_t err;

	cuCtxSetCurrent(w->cuctx);

	while (submitted < w->qd && submitted < w->nops) {
		err = opends_async_read(w->fh, w->bufs[submitted], w->block,
		                        (off_t)off, 0, &w->futures[submitted]);
		if (err.err != OPENDS_SUCCESS) {
			fprintf(stderr, "submit: %s\n",
			        opends_op_status_error(err.err));
			w->failed = 1;
			break;
		}
		submitted++;
		off += w->stride;
		if (off + w->block > w->span)
			off = w->start_off;
	}

	while (w->done < submitted) {
		unsigned slot = (unsigned)(w->done % w->qd);
		ssize_t n = opends_async_await(&w->futures[slot]);

		if (n != (ssize_t)w->block)
			w->errors++;
		w->done++;

		if (!w->failed && submitted < w->nops) {
			err = opends_async_read(w->fh, w->bufs[slot], w->block,
			                        (off_t)off, 0,
			                        &w->futures[slot]);
			if (err.err != OPENDS_SUCCESS) {
				fprintf(stderr, "resubmit: %s\n",
				        opends_op_status_error(err.err));
				w->failed = 1;
				continue;
			}
			submitted++;
			off += w->stride;
			if (off + w->block > w->span)
				off = w->start_off;
		}
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	if (argc != 5 && argc != 6) {
		fprintf(stderr,
		        "usage: %s <file-on-mount> <block_bytes> <qd> "
		        "<total_mib> [threads]\n",
		        argv[0]);
		return 1;
	}

	size_t block = strtoull(argv[2], NULL, 0);
	unsigned qd = (unsigned)strtoul(argv[3], NULL, 0);
	uint64_t total = strtoull(argv[4], NULL, 0) << 20;
	unsigned nthreads = argc == 6 ? (unsigned)strtoul(argv[5], NULL, 0) : 1;

	if (!block || !qd || qd > MAX_QD || !total || !nthreads ||
	    nthreads > MAX_THREADS) {
		fprintf(stderr, "bad block/qd/total/threads\n");
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
	struct async_worker *ws = calloc(nthreads, sizeof(*ws));
	if (!ws)
		goto out;

	uint64_t nops = total / block;

	for (unsigned t = 0; t < nthreads; t++) {
		struct async_worker *w = &ws[t];

		w->cuctx = a.cuctx;
		w->fh = a.fh;
		w->block = block;
		w->qd = qd;
		w->span = span;
		w->start_off = (uint64_t)t * block;
		w->stride = (uint64_t)nthreads * block;
		w->nops = nops / nthreads + (t < nops % nthreads);
		w->bufs = calloc(qd, sizeof(*w->bufs));
		w->futures = calloc(qd, sizeof(*w->futures));
		if (!w->bufs || !w->futures)
			goto out;
		for (unsigned i = 0; i < qd; i++) {
			w->bufs[i] = opends_alloc(block);
			if (!w->bufs[i]) {
				fprintf(stderr,
				        "opends_alloc(%zu) failed at %u/%u\n",
				        block, t, i);
				goto out;
			}
		}
	}

	uint64_t t0 = now_ns();

	for (unsigned t = 0; t < nthreads; t++)
		pthread_create(&ws[t].tid, NULL, async_worker_fn, &ws[t]);
	for (unsigned t = 0; t < nthreads; t++)
		pthread_join(ws[t].tid, NULL);

	uint64_t dt = now_ns() - t0;

	uint64_t done = 0;
	uint64_t errors = 0;
	int failed = 0;
	for (unsigned t = 0; t < nthreads; t++) {
		done += ws[t].done;
		errors += ws[t].errors;
		failed |= ws[t].failed;
	}
	if (failed || !done)
		goto out;

	double secs = (double)dt / 1e9;
	double gbs = (double)(done * block) / 1e9 / secs;
	double iops = (double)done / secs;
	double lat_us =
	        (double)dt / 1e3 / (double)done * (double)(qd * nthreads);

	printf("block=%zu qd=%u threads=%u ops=%" PRIu64 " errors=%" PRIu64
	       " time=%.3fs %.3f GB/s %.0f IOPS avg_lat=%.1fus\n",
	       block, qd, nthreads, done, errors, secs, gbs, iops, lat_us);

	rc = errors ? 1 : 0;
out:
	if (ws)
		for (unsigned t = 0; t < nthreads; t++) {
			if (ws[t].bufs)
				for (unsigned i = 0; i < qd; i++)
					if (ws[t].bufs[i])
						opends_free(ws[t].bufs[i]);
			free(ws[t].bufs);
			free(ws[t].futures);
		}
	free(ws);
	aisio_homi_teardown(&a);
	return rc;
}
