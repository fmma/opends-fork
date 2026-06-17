/*
 * bench_thread_scaling.c - read-throughput vs thread count, backend- and
 * dataset-agnostic.
 *
 * N threads each loop open(file) + ds_file_handle_register + ds_file_read
 * (--read-bytes at --read-offset) + deregister + close, into a pool of registered
 * GPU buffers (cycled per read). Files are handed out by a shared atomic cursor
 * so each file is read exactly once across all threads -- a single-pass sweep
 * with no reread, so a working set larger than the SSD's internal cache stays
 * NAND-bound. The run ends when the dataset is exhausted; --timeout is a
 * watchdog against a wedged read, not the run length. The linked opends backend
 * (gds or aisio) decides the path; build twice and run each under its own device
 * setup to compare. Reports aggregate GiB/s and per-read latency p50/p99.
 *
 * Works on any dataset: point --data at a directory tree of files and pick
 * --read-bytes / --read-offset (read-offset 0 reads from the start; nonzero skips a
 * per-file header, e.g. LMCache's 4096-byte KV metadata).
 *
 * usage: bench_thread_scaling --data DIR --read-bytes N --threads N \
 *            --timeout SECS --read-offset N --buffers N [--serialize]
 * env:   aisio needs OPENDS_HOMI_DEV / OPENDS_HOMI_SOCKET (and OPENDS_AISIO_REACTORS
 *        selects the reactor count); gds needs nvidia_fs. These are the linked
 *        library's runtime interface, not bench knobs.
 */

#define _GNU_SOURCE

#include "opends.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include <fcntl.h>
#include <ftw.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef LOADBENCH_BACKEND
#define LOADBENCH_BACKEND "unknown"
#endif
#ifdef LOADBENCH_ODIRECT
#define OPEN_FLAGS (O_RDONLY | O_DIRECT)
#else
#define OPEN_FLAGS (O_RDONLY)
#endif

#define LBA_ALIGN 4096ULL
#define REG_PAGE (64ULL * 1024)
#define HIST_US 200000
#define MAX_FILES 65536

static struct {
	char *path;
	uint64_t size;
} g_files[MAX_FILES];
static int g_nfiles;
static uint64_t g_min_size;

static int
collect(const char *path, const struct stat *st, int type, struct FTW *ftw)
{
	(void)ftw;
	if (type == FTW_F && S_ISREG(st->st_mode) &&
	    (uint64_t)st->st_size >= g_min_size && g_nfiles < MAX_FILES) {
		g_files[g_nfiles].path = strdup(path);
		g_files[g_nfiles].size = (uint64_t)st->st_size;
		g_nfiles++;
	}
	return 0;
}

static uint64_t
align_down(uint64_t x, uint64_t a)
{
	return x & ~(a - 1);
}

static uint64_t
align_up(uint64_t x, uint64_t a)
{
	return (x + a - 1) & ~(a - 1);
}

static double
now_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

struct worker {
	pthread_t thread;
	int tid;
	void **bufs;   /* n_buffers separately-registered GPU buffers, cycled */
	int n_buffers; /* per read, to defeat per-buffer DMA-descriptor reuse */
	size_t read_bytes;
	uint64_t reads;
	uint64_t bytes;
	uint64_t errors;
	uint64_t lat_ns_sum;
	uint64_t *hist;
};

static _Atomic int g_go;
static _Atomic int g_stop;
static _Atomic int g_next;
static pthread_mutex_t g_serial_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_serialize;
static uint64_t g_read_offset;
static double g_timeout_secs;
static int g_limit;

static void *
watchdog_main(void *arg)
{
	(void)arg;
	struct timespec ts = {
	        .tv_sec = (time_t)g_timeout_secs,
	        .tv_nsec = (long)((g_timeout_secs - (double)(time_t)g_timeout_secs)
	                          * 1e9),
	};
	nanosleep(&ts, NULL);
	atomic_store(&g_stop, 1);
	return NULL;
}

static void *
worker_main(void *arg)
{
	struct worker *w = arg;
	uint64_t it = 0;

	while (!atomic_load_explicit(&g_go, memory_order_acquire))
		sched_yield();

	while (!atomic_load_explicit(&g_stop, memory_order_relaxed)) {
		int fi = atomic_fetch_add_explicit(&g_next, 1,
		                                   memory_order_relaxed);
		if (fi >= g_limit)
			break; /* each file read once, no reread (capped at g_limit) */
		it++;

		/* Issue the read exactly as configured: read_bytes at read_offset, for
		 * every file. An out-of-bounds request (read_offset + read_bytes past
		 * EOF) is left to the backend to reject and counts as an error below;
		 * the bench never clamps or skips, so it can't silently alter the
		 * requested read structure. */
		size_t sz = w->read_bytes;
		uint64_t off = g_read_offset;

		void *buf = w->bufs[(it - 1) % (uint64_t)w->n_buffers];
		if (g_serialize)
			pthread_mutex_lock(&g_serial_lock);
		double t0 = now_sec();
		ssize_t n = -1;
		ds_file_handle_t fh;
		int fd = open(g_files[fi].path, OPEN_FLAGS);
		if (fd >= 0) {
			if (ds_file_handle_register(&fh, fd).err ==
			    DS_FILE_SUCCESS) {
				n = ds_file_read(fh, buf, sz, (off_t)off, 0);
				ds_file_handle_deregister(fh);
			}
			close(fd);
		}
		double t1 = now_sec();
		if (g_serialize)
			pthread_mutex_unlock(&g_serial_lock);

		if (n != (ssize_t)sz) {
			w->errors++;
			continue;
		}
		uint64_t ns = (uint64_t)((t1 - t0) * 1e9);
		w->lat_ns_sum += ns;
		uint64_t us = ns / 1000;
		w->hist[us < HIST_US ? us : HIST_US]++;
		w->reads++;
		w->bytes += (uint64_t)n;
	}
	return NULL;
}

static double
percentile(const uint64_t *hist, uint64_t total, double p)
{
	if (!total)
		return 0.0;
	uint64_t target = (uint64_t)(p * (double)total);
	uint64_t acc = 0;
	for (int i = 0; i <= HIST_US; i++) {
		acc += hist[i];
		if (acc >= target)
			return (double)i;
	}
	return (double)HIST_US;
}

int
main(int argc, char **argv)
{
	const char *dir = NULL;
	size_t read_bytes = 0;
	int threads = 0;
	double seconds = 0.0;
	long buffers_total = 0; /* distinct GPU buffers, divided across threads */
	long read_offset = -1;     /* per-file read offset; 0 reads from the start */
	int bad = 0;

	for (int i = 1; i < argc && !bad; i++) {
		const char *a = argv[i];
		const char *v = (i + 1 < argc) ? argv[i + 1] : NULL;
		if (!strcmp(a, "--serialize")) {
			g_serialize = 1;
		} else if (!v) {
			fprintf(stderr, "missing value for %s\n", a);
			bad = 1;
		} else if (!strcmp(a, "--data")) {
			dir = v; i++;
		} else if (!strcmp(a, "--read-bytes")) {
			read_bytes = (size_t)strtoull(v, NULL, 0); i++;
		} else if (!strcmp(a, "--threads")) {
			threads = atoi(v); i++;
		} else if (!strcmp(a, "--timeout")) {
			seconds = strtod(v, NULL); i++;
		} else if (!strcmp(a, "--read-offset")) {
			read_offset = strtol(v, NULL, 0); i++;
		} else if (!strcmp(a, "--buffers")) {
			buffers_total = strtol(v, NULL, 0); i++;
		} else {
			fprintf(stderr, "unknown arg: %s\n", a);
			bad = 1;
		}
	}

	if (bad || !dir || read_bytes == 0 || threads < 1 || threads > 256 ||
	    seconds <= 0.0 || buffers_total < 1 || read_offset < 0) {
		fprintf(stderr,
		        "usage: %s --data DIR --read-bytes N --threads N --timeout SECS\n"
		        "          --read-offset N --buffers N [--serialize]\n",
		        argv[0]);
		return 2;
	}

	read_bytes = (size_t)align_down(read_bytes, LBA_ALIGN);
	g_read_offset = (uint64_t)read_offset;
	if (read_bytes < LBA_ALIGN) {
		fprintf(stderr, "--read-bytes must be >= 4096\n");
		return 2;
	}
	if (g_read_offset & (LBA_ALIGN - 1)) {
		fprintf(stderr, "--read-offset must be 4096-aligned\n");
		return 2;
	}

	/* --buffers is a fixed pool divided across threads (per-thread = total/threads),
	 * so the in-flight buffer working set stays constant as thread count varies --
	 * isolating thread scaling from per-buffer cuFile contention. Each thread gets a
	 * private slice; no cross-thread buffer sharing. */
	int n_buffers = buffers_total < threads ? 1 : (int)(buffers_total / threads);

	g_min_size = LBA_ALIGN;
	if (nftw(dir, collect, 16, FTW_PHYS) < 0 || g_nfiles == 0) {
		fprintf(stderr, "no readable files under %s\n", dir);
		return 1;
	}

	g_limit = g_nfiles;

	cuInit(0);
	CUdevice cudev;
	CUcontext cuctx;
	cuDeviceGet(&cudev, 0);
	if (cuCtxCreate(&cuctx, 0, cudev) != CUDA_SUCCESS) {
		fprintf(stderr, "cuCtxCreate failed\n");
		return 1;
	}

	ds_file_error_t derr = ds_file_driver_open();
	if (derr.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "driver_open(%s): %s\n", LOADBENCH_BACKEND,
		        ds_file_op_status_error(derr.err));
		return 1;
	}

	size_t stride = align_up(read_bytes, REG_PAGE);
	int nbuf = threads * n_buffers;
	void **bufs = calloc((size_t)nbuf, sizeof(*bufs));
	for (int j = 0; j < nbuf; j++) {
		if (cudaMalloc(&bufs[j], stride) != cudaSuccess) {
			fprintf(stderr, "cudaMalloc(%zu) failed\n", stride);
			return 1;
		}
		if (ds_file_buf_register(bufs[j], stride, 0).err !=
		    DS_FILE_SUCCESS) {
			fprintf(stderr, "buf_register failed (buf %d)\n", j);
			return 1;
		}
	}

	struct worker *ws = calloc((size_t)threads, sizeof(*ws));
	atomic_store(&g_stop, 0);
	atomic_store(&g_go, 0);
	atomic_store(&g_next, 0);
	int started = 0;
	for (int i = 0; i < threads; i++) {
		struct worker *w = &ws[i];
		w->tid = i;
		w->bufs = &bufs[i * n_buffers];
		w->n_buffers = n_buffers;
		w->read_bytes = read_bytes;
		w->hist = calloc(HIST_US + 1, sizeof(uint64_t));
		if (!w->hist ||
		    pthread_create(&w->thread, NULL, worker_main, w) != 0)
			break;
		started++;
	}
	if (started != threads) {
		atomic_store(&g_stop, 1);
		atomic_store(&g_go, 1);
		for (int i = 0; i < started; i++)
			pthread_join(ws[i].thread, NULL);
		fprintf(stderr, "thread spawn failed\n");
		return 1;
	}

	/* Watchdog only: workers self-terminate when the cursor exhausts the
	 * dataset (single pass, no reread). The detached timer trips g_stop only
	 * if a read wedges, so it never extends a clean run. */
	g_timeout_secs = seconds;
	pthread_t watchdog;
	if (pthread_create(&watchdog, NULL, watchdog_main, NULL) == 0)
		pthread_detach(watchdog);

	double t_start = now_sec();
	atomic_store(&g_go, 1);
	for (int i = 0; i < threads; i++)
		pthread_join(ws[i].thread, NULL);
	double elapsed = now_sec() - t_start;
	atomic_store(&g_stop, 1);

	uint64_t tot_reads = 0, tot_bytes = 0, tot_err = 0, tot_lat = 0;
	uint64_t *hist = calloc(HIST_US + 1, sizeof(uint64_t));
	for (int i = 0; i < threads; i++) {
		tot_reads += ws[i].reads;
		tot_bytes += ws[i].bytes;
		tot_err += ws[i].errors;
		tot_lat += ws[i].lat_ns_sum;
		for (int j = 0; j <= HIST_US; j++)
			hist[j] += ws[i].hist[j];
		free(ws[i].hist);
	}

	double gib = (double)tot_bytes / (1024.0 * 1024.0 * 1024.0);
	double mean_us = tot_reads
	                         ? (double)tot_lat / (double)tot_reads / 1000.0
	                         : 0.0;
	printf("RESULT backend=%s reactors=%s serialize=%d read_offset=%llu "
	       "threads=%d buffers=%d buf_total=%d nfiles=%d read_bytes=%zu secs=%.3f reads=%llu GiB_s=%.3f "
	       "lat_us_mean=%.1f lat_us_p50=%.0f lat_us_p99=%.0f errors=%llu\n",
	       LOADBENCH_BACKEND, getenv("OPENDS_AISIO_REACTORS")
	                                  ? getenv("OPENDS_AISIO_REACTORS")
	                                  : "-",
	       g_serialize, (unsigned long long)g_read_offset, threads, n_buffers,
	       nbuf, g_limit, read_bytes, elapsed,
	       (unsigned long long)tot_reads, gib / elapsed, mean_us,
	       percentile(hist, tot_reads, 0.50),
	       percentile(hist, tot_reads, 0.99),
	       (unsigned long long)tot_err);
	fflush(stdout);

	free(hist);
	free(ws);
	for (int j = 0; j < nbuf; j++) {
		ds_file_buf_deregister(bufs[j]);
		cudaFree(bufs[j]);
	}
	free(bufs);
	ds_file_driver_close();
	cuCtxDestroy(cuctx);
	return tot_err ? 1 : 0;
}
