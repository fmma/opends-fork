/*
 * test_aisio_homi.h - Shared setup for aisio tests against a live HOMI.
 *
 * The aisio backend no longer owns the controller or takes injected
 * extents: it attaches I/O qpairs from a running HOMI daemon and resolves
 * file extents via FIEMAP on a real file. These tests therefore run while
 * the HOMI/qublk stack is up and the test filesystem is mounted (over
 * /dev/ublkbN). The harness (tasks/test.yaml) brings the stack up, exports
 * OPENDS_HOMI_DEV / OPENDS_HOMI_SOCKET, and passes the path of a real file
 * on the mount.
 *
 * Each test opens that file, registers it (which FIEMAPs it and attaches
 * qpairs), and runs the backend-agnostic test logic against it.
 */
#ifndef TEST_AISIO_HOMI_H_
#define TEST_AISIO_HOMI_H_

#include "opends.h"

#include <cuda.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

struct aisio_homi {
	CUcontext cuctx;
	int fd;
	ds_file_handle_t fh;
};

/*
 * Create a CUDA context (the upcie-cuda backend needs one current for
 * device init and buffer allocation), open the aisio driver against the
 * HOMI daemon named by the environment, open `path`, and register it.
 *
 * Returns 0 on success with *out filled in, or -1 on failure (a message is
 * printed). On failure nothing needs tearing down.
 */
static inline int
aisio_homi_setup(const char *path, struct aisio_homi *out)
{
	CUdevice cudev;

	out->cuctx = NULL;
	out->fd = -1;
	out->fh = NULL;

	cuInit(0);
	cuDeviceGet(&cudev, 0);
	if (cuCtxCreate(&out->cuctx, 0, cudev) != CUDA_SUCCESS) {
		fprintf(stderr, "cuCtxCreate failed\n");
		return -1;
	}

	ds_file_error_t err = ds_file_driver_open();
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr,
		        "driver_open: %s (is homid running and "
		        "OPENDS_HOMI_DEV set?)\n",
		        ds_file_op_status_error(err.err));
		cuCtxDestroy(out->cuctx);
		return -1;
	}

	out->fd = open(path, O_RDONLY);
	if (out->fd < 0) {
		fprintf(stderr, "open(%s) failed\n", path);
		ds_file_driver_close();
		cuCtxDestroy(out->cuctx);
		return -1;
	}

	err = ds_file_handle_register(&out->fh, out->fd);
	if (err.err != DS_FILE_SUCCESS) {
		fprintf(stderr, "handle_register: %s\n",
		        ds_file_op_status_error(err.err));
		close(out->fd);
		ds_file_driver_close();
		cuCtxDestroy(out->cuctx);
		return -1;
	}

	return 0;
}

static inline void
aisio_homi_teardown(struct aisio_homi *a)
{
	if (a->fh)
		ds_file_handle_deregister(a->fh);
	ds_file_driver_close();
	if (a->fd >= 0)
		close(a->fd);
	if (a->cuctx)
		cuCtxDestroy(a->cuctx);
}

#endif /* TEST_AISIO_HOMI_H_ */
