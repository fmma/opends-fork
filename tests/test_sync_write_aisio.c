/* SPDX-License-Identifier: BSD-3-Clause */
#define _GNU_SOURCE

#include "test_aisio_homi.h"
#include "test_cuda_common.h"
#include "test_write_homi.h"

#include <cuda.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static ssize_t
sync_submit_write(struct write_homi_env *e, void *gpu, size_t size, off_t foff)
{
	return opends_sync_write(e->fh, gpu, size, foff, 0);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <scratch-file-on-mount>\n", argv[0]);
		return 1;
	}
	const char *path = argv[1];

	struct aisio_homi a;
	if (aisio_homi_setup_flags(path, O_RDWR | O_CREAT | O_TRUNC, &a) < 0)
		return 1;

	fprintf(stderr, "opends_sync_write tests (aisio backend, HOMI)\n");

	struct write_homi_env env = {
	        .fh = a.fh,
	        .stream = NULL,
	        .submit_write = sync_submit_write,
	        .mode_label = "sync",
	};
	int failed = run_write_homi_tests(&env);

	aisio_homi_teardown(&a);
	unlink(path);

	if (failed)
		fprintf(stderr, "%d test(s) failed\n", failed);
	else
		fprintf(stderr, "all ok\n");
	return failed ? 1 : 0;
}
