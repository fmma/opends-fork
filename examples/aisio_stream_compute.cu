// SPDX-License-Identifier: BSD-3-Clause
/*
 * Compute kernels and stream reads on one CUDA stream, with the host's part
 * measured.
 *
 * Each iteration enqueues: a kernel that overwrites the buffer, a stamp of the
 * GPU clock, opends_stream_read of the file into the buffer, another stamp,
 * and a kernel that sums the buffer. Everything is enqueued up front. The host
 * then sleeps, and samples every thread's CPU time from /proc before and
 * after. The sums must equal the host's sum over the file, which shows each
 * read landed between its two kernels; the per-thread CPU times show which
 * host threads took part while the chain ran; the stamps bound each read on
 * the GPU timeline.
 *
 * Usage: aisio_stream_compute <file-on-mount> [iters] [host-sleep-ms]
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <opends.h>

#include <cuda.h>
#include <cuda_runtime.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_READ_BYTES (256u << 20)
#define MAX_THREADS 64
#define THREADS 256
#define FILL_PATTERN 0xA5A5A5A5A5A5A5A5ull

struct thread_cpu {
	int tid;
	char comm[32];
	uint64_t run_ns;
};

struct cpu_snapshot {
	struct thread_cpu t[MAX_THREADS];
	int n;
};

static __global__ void
fill_kernel(uint64_t *p, size_t n, uint64_t v)
{
	size_t stride = (size_t)gridDim.x * blockDim.x;

	for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < n;
	     i += stride)
		p[i] = v;
}

static __global__ void
stamp_kernel(uint64_t *ts)
{
	uint64_t t;

	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	*ts = t;
}

static __global__ void
sum_kernel(const uint64_t *p, size_t n, unsigned long long *out)
{
	__shared__ unsigned long long sh[THREADS];
	size_t stride = (size_t)gridDim.x * blockDim.x;
	unsigned long long acc = 0;

	for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < n;
	     i += stride)
		acc += p[i];
	sh[threadIdx.x] = acc;
	__syncthreads();
	for (unsigned s = THREADS / 2; s > 0; s >>= 1) {
		if (threadIdx.x < s)
			sh[threadIdx.x] += sh[threadIdx.x + s];
		__syncthreads();
	}
	if (threadIdx.x == 0)
		atomicAdd(out, sh[0]);
}

static double
now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static void
sleep_ms(long ms)
{
	struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};

	nanosleep(&ts, NULL);
}

/* Time on CPU in ns (schedstat) and the name of one thread. */
static int
read_thread_cpu(int tid, struct thread_cpu *out)
{
	char path[64];
	FILE *f;
	int n;

	snprintf(path, sizeof(path), "/proc/self/task/%d/schedstat", tid);
	f = fopen(path, "r");
	if (!f)
		return -1;
	n = fscanf(f, "%" SCNu64, &out->run_ns);
	fclose(f);
	if (n != 1)
		return -1;

	snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
	f = fopen(path, "r");
	if (!f)
		return -1;
	if (!fgets(out->comm, sizeof(out->comm), f))
		out->comm[0] = '\0';
	fclose(f);
	out->comm[strcspn(out->comm, "\n")] = '\0';
	out->tid = tid;
	return 0;
}

static void
snapshot_cpu(struct cpu_snapshot *s)
{
	DIR *d = opendir("/proc/self/task");
	struct dirent *e;

	s->n = 0;
	if (!d)
		return;
	while ((e = readdir(d)) && s->n < MAX_THREADS) {
		if (e->d_name[0] == '.')
			continue;
		if (read_thread_cpu(atoi(e->d_name), &s->t[s->n]) == 0)
			s->n++;
	}
	closedir(d);
}

/* 0 for a thread that did not exist yet. */
static uint64_t
run_ns_of(const struct cpu_snapshot *s, int tid)
{
	for (int i = 0; i < s->n; i++)
		if (s->t[i].tid == tid)
			return s->t[i].run_ns;
	return 0;
}

/* Per-thread CPU time between two snapshots; returns the total in ms. */
static double
report_cpu(const struct cpu_snapshot *a, const struct cpu_snapshot *b,
           double wall_ms)
{
	double total = 0;

	for (int i = 0; i < b->n; i++) {
		double ms = (b->t[i].run_ns - run_ns_of(a, b->t[i].tid)) / 1e6;

		total += ms;
		printf("    tid %-7d %-16s %9.3f ms\n", b->t[i].tid,
		       b->t[i].comm, ms);
	}
	printf("    %-24s %9.3f ms  (%.2f%% of one core over %.0f ms)\n",
	       "total", total, 100.0 * total / wall_ms, wall_ms);
	return total;
}

static int
host_sum(int fd, size_t nbytes, unsigned long long *out)
{
	void *host = NULL;
	const uint64_t *w;
	unsigned long long acc = 0;
	size_t done = 0;

	if (posix_memalign(&host, 4096, nbytes))
		return -1;
	while (done < nbytes) {
		ssize_t n = pread(fd, (char *)host + done, nbytes - done, done);

		if (n <= 0) {
			free(host);
			return -1;
		}
		done += (size_t)n;
	}
	w = (const uint64_t *)host;
	for (size_t i = 0; i < nbytes / sizeof(uint64_t); i++)
		acc += w[i];
	free(host);
	*out = acc;
	return 0;
}

int
main(int argc, char **argv)
{
	const char *path;
	int iters = 20;
	long host_sleep_ms = 1500;
	const char *gpu_env = getenv("OPENDS_AISIO_GPU_INITIATED");
	bool gpu_engine = gpu_env && gpu_env[0] && gpu_env[0] != '0';
	CUdevice cudev;
	CUcontext cuctx;
	CUstream stream;
	CUresult cres;
	opends_error_t err;
	opends_handle_t fh = NULL;
	struct stat st;
	size_t nbytes, n_words;
	unsigned grid;
	unsigned long long ref = 0;
	void *buf;
	uint64_t *ts_dev, *ts;
	unsigned long long *sums_dev, *sums;
	size_t *sz;
	off_t *foff, *boff;
	ssize_t *bytes;
	struct cpu_snapshot s0, s1, s2;
	double t0, t1, t_wake, t2;
	bool done_asleep;
	int polls = 0, bad = 0, fd, rc = 1;

	if (argc < 2 || argc > 4) {
		fprintf(stderr,
		        "usage: %s <file-on-mount> [iters] [host-sleep-ms]\n",
		        argv[0]);
		return 2;
	}
	path = argv[1];
	if (argc > 2)
		iters = atoi(argv[2]);
	if (argc > 3)
		host_sleep_ms = atol(argv[3]);
	if (iters < 1 || host_sleep_ms < 0) {
		fprintf(stderr, "bad iters or sleep\n");
		return 2;
	}

	cuInit(0);
	cuDeviceGet(&cudev, 0);
#if CUDA_VERSION >= 13000
	cres = cuCtxCreate(&cuctx, NULL, 0, cudev);
#else
	cres = cuCtxCreate(&cuctx, 0, cudev);
#endif
	if (cres != CUDA_SUCCESS) {
		fprintf(stderr, "cuCtxCreate failed: %d\n", (int)cres);
		return 1;
	}

	err = opends_driver_open();
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr,
		        "driver_open: %s (is the homi stack running?)\n",
		        opends_op_status_error(err.err));
		return 1;
	}

	fd = open(path, O_RDONLY | O_DIRECT);
	if (fd < 0 || fstat(fd, &st) < 0) {
		perror(path);
		goto out_driver;
	}
	nbytes = (size_t)st.st_size;
	if (nbytes > MAX_READ_BYTES)
		nbytes = MAX_READ_BYTES;
	nbytes &= ~(size_t)4095;
	if (!nbytes) {
		fprintf(stderr, "%s: too small\n", path);
		goto out_fd;
	}
	n_words = nbytes / sizeof(uint64_t);
	grid = (unsigned)((n_words + THREADS - 1) / THREADS);
	if (grid > 1024)
		grid = 1024;

	err = opends_handle_register(&fh, fd);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        opends_op_status_error(err.err));
		goto out_fd;
	}
	if (host_sum(fd, nbytes, &ref) < 0) {
		fprintf(stderr, "host read of %s failed\n", path);
		goto out_handle;
	}

	buf = opends_alloc(nbytes);
	if (!buf) {
		fprintf(stderr, "opends_alloc(%zu) failed\n", nbytes);
		goto out_handle;
	}
	if (cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING) != CUDA_SUCCESS) {
		fprintf(stderr, "cuStreamCreate failed\n");
		goto out_buf;
	}
	err = opends_stream_register(stream, 0);
	if (err.err != OPENDS_SUCCESS) {
		fprintf(stderr, "stream_register: %s\n",
		        opends_op_status_error(err.err));
		goto out_stream;
	}
	if (cudaMalloc((void **)&ts_dev, 2 * iters * sizeof(*ts_dev)) ||
	    cudaMalloc((void **)&sums_dev, iters * sizeof(*sums_dev)) ||
	    cudaMemset(sums_dev, 0, iters * sizeof(*sums_dev))) {
		fprintf(stderr, "cudaMalloc failed\n");
		goto out_stream;
	}
	ts = (uint64_t *)calloc(2 * iters, sizeof(*ts));
	sums = (unsigned long long *)calloc(iters, sizeof(*sums));
	sz = (size_t *)calloc(iters, sizeof(*sz));
	foff = (off_t *)calloc(iters, sizeof(*foff));
	boff = (off_t *)calloc(iters, sizeof(*boff));
	bytes = (ssize_t *)calloc(iters, sizeof(*bytes));
	if (!ts || !sums || !sz || !foff || !boff || !bytes) {
		fprintf(stderr, "calloc failed\n");
		goto out_stream;
	}

	printf("aisio_stream_compute: %d x (fill, read %zu MiB, sum) on one "
	       "stream, GPU engine %s\n",
	       iters, nbytes >> 20, gpu_engine ? "on" : "off");

	snapshot_cpu(&s0);
	t0 = now_ms();
	for (int i = 0; i < iters; i++) {
		fill_kernel<<<grid, THREADS, 0, (cudaStream_t)stream>>>(
		        (uint64_t *)buf, n_words, FILL_PATTERN);
		stamp_kernel<<<1, 1, 0, (cudaStream_t)stream>>>(&ts_dev[2 * i]);
		sz[i] = nbytes;
		err = opends_stream_read(fh, buf, &sz[i], &foff[i], &boff[i],
		                         &bytes[i], (opends_stream_t)stream);
		if (err.err != OPENDS_SUCCESS) {
			fprintf(stderr, "stream_read %d: %s\n", i,
			        opends_op_status_error(err.err));
			goto out_stream;
		}
		stamp_kernel<<<1, 1, 0, (cudaStream_t)stream>>>(
		        &ts_dev[2 * i + 1]);
		sum_kernel<<<grid, THREADS, 0, (cudaStream_t)stream>>>(
		        (const uint64_t *)buf, n_words, &sums_dev[i]);
	}
	t1 = now_ms();
	snapshot_cpu(&s1);

	/* Nothing on this thread drives the chain from here on. */
	sleep_ms(host_sleep_ms);
	cres = cuStreamQuery(stream);
	done_asleep = cres == CUDA_SUCCESS;
	t_wake = now_ms();
	while (cres == CUDA_ERROR_NOT_READY) {
		sleep_ms(1);
		polls++;
		cres = cuStreamQuery(stream);
	}
	t2 = now_ms();
	snapshot_cpu(&s2);
	if (cres != CUDA_SUCCESS) {
		fprintf(stderr, "stream failed: %d\n", (int)cres);
		goto out_stream;
	}

	cudaMemcpy(ts, ts_dev, 2 * iters * sizeof(*ts), cudaMemcpyDeviceToHost);
	cudaMemcpy(sums, sums_dev, iters * sizeof(*sums),
	           cudaMemcpyDeviceToHost);
	for (int i = 0; i < iters; i++) {
		if (sums[i] != ref || bytes[i] != (ssize_t)nbytes) {
			printf("  iteration %d: sum %#llx vs %#llx, bytes_read "
			       "%zd\n",
			       i, sums[i], ref, bytes[i]);
			bad++;
		}
	}
	printf("  data: %s\n", bad ? "MISMATCH"
	                           : "every sum matches the host's, every "
	                             "bytes_read is the full size");

	printf("  enqueue: %.1f ms wall\n", t1 - t0);
	report_cpu(&s0, &s1, t1 - t0);

	printf("  run: host slept %ld ms, chain %s while it slept%s\n",
	       host_sleep_ms, done_asleep ? "finished" : "was still running",
	       done_asleep ? "" : "; polled until done");
	if (!done_asleep)
		printf("  run: finished %.1f ms after waking (%d polls)\n",
		       t2 - t_wake, polls);
	printf("  run: %.1f ms wall from the last enqueue to completion\n",
	       t2 - t1);
	report_cpu(&s1, &s2, t2 - t1);

	{
		double lo = 1e300, hi = 0, acc = 0, gap = 0;

		for (int i = 0; i < iters; i++) {
			double w = (ts[2 * i + 1] - ts[2 * i]) / 1e6;

			if (w < lo)
				lo = w;
			if (w > hi)
				hi = w;
			acc += w;
			if (i)
				gap += (ts[2 * i] - ts[2 * i - 1]) / 1e6;
		}
		printf("  GPU timeline: read window min/avg/max %.2f/%.2f/%.2f "
		       "ms "
		       "(%.2f GB/s avg); sum + next fill %.2f ms avg; whole "
		       "chain "
		       "%.1f ms\n",
		       lo, acc / iters, hi, nbytes / (acc / iters) / 1e6,
		       iters > 1 ? gap / (iters - 1) : 0.0,
		       (ts[2 * iters - 1] - ts[0]) / 1e6);
	}
	rc = bad ? 1 : 0;

out_stream:
	cuStreamDestroy(stream);
out_buf:
	opends_free(buf);
out_handle:
	opends_handle_deregister(fh);
out_fd:
	close(fd);
out_driver:
	opends_driver_close();
	cuCtxDestroy(cuctx);
	return rc;
}
