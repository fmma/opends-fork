/*
 * ds_file_gds.c - GDS backend using NVIDIA cuFile.
 *
 * Thin wrapper around the cuFile API. Buffers are allocated on the GPU
 * with cudaMalloc and registered with cuFileBufRegister for DMA. Batch
 * and async operations delegate directly to cuFile.
 *
 * Requires: NVIDIA CUDA toolkit and cuFile (GDS) library.
 * Tested against cuFile API 1.x (GDS 1.4+).
 */

#define _GNU_SOURCE

#include "ds_file_internal.h"

#include <cufile.h>
#include <cuda_runtime_api.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

_Static_assert((int)DS_FILE_SUCCESS == (int)CU_FILE_SUCCESS,
               "ds_file/cuFile error code mismatch: SUCCESS");
_Static_assert((int)DS_FILE_DRIVER_NOT_INITIALIZED ==
                       (int)CU_FILE_DRIVER_NOT_INITIALIZED,
               "ds_file/cuFile error code mismatch: DRIVER_NOT_INITIALIZED");
_Static_assert((int)DS_FILE_INTERNAL_ERROR == (int)CU_FILE_INTERNAL_ERROR,
               "ds_file/cuFile error code mismatch: INTERNAL_ERROR");

struct gds_handle {
	CUfileHandle_t cufh;
};

static bool driver_open;
static long use_count;

static ds_file_error_t
from_cufile_error(CUfileError_t e)
{
	return (ds_file_error_t){(ds_file_op_error_t)e.err,
	                         (ds_result_t)e.cu_err};
}

/* ------------------------------------------------------------------ */
/*  Driver lifecycle                                                   */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_driver_open(void)
{
	if (driver_open)
		return ds_file_err(DS_FILE_DRIVER_ALREADY_OPEN);
	CUfileError_t err = cuFileDriverOpen();
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	driver_open = true;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_close(void)
{
	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	CUfileError_t err = cuFileDriverClose();
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	driver_open = false;
	return ds_file_ok();
}

long
ds_file_use_count(void)
{
	return use_count;
}

ds_file_error_t
ds_file_driver_get_properties(ds_file_drv_props_t *props)
{
	if (!props)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	CUfileDrvProps_t cu_props;
	CUfileError_t err = cuFileDriverGetProperties(&cu_props);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);

	memset(props, 0, sizeof(*props));
	props->major_version = cu_props.nvfs.major_version;
	props->minor_version = cu_props.nvfs.minor_version;
	props->max_direct_io_size = cu_props.nvfs.max_direct_io_size;
	props->max_batch_io_size = cu_props.max_batch_io_size;
	props->max_batch_io_timeout_msecs = cu_props.max_batch_io_timeout_msecs;
	return ds_file_ok();
}

ds_file_error_t
ds_file_driver_set_max_direct_io_size(size_t max_direct_io_size)
{
	CUfileError_t err = cuFileDriverSetMaxDirectIOSize(max_direct_io_size);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_get_version(unsigned *major, unsigned *minor, unsigned *patch)
{
	CUfileDrvProps_t cu_props;
	CUfileError_t err = cuFileDriverGetProperties(&cu_props);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);

	if (major)
		*major = cu_props.nvfs.major_version;
	if (minor)
		*minor = cu_props.nvfs.minor_version;
	if (patch)
		*patch = 0;
	return ds_file_ok();
}

/* ------------------------------------------------------------------ */
/*  Handle registration                                                */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_handle_register(ds_file_handle_t *fh, int fd)
{
	if (!fh || fd < 0)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	struct gds_handle *h = malloc(sizeof(*h));
	if (!h)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	CUfileDescr_t desc;
	memset(&desc, 0, sizeof(desc));
	desc.type = CU_FILE_HANDLE_TYPE_OPAQUE_FD;
	desc.handle.fd = fd;

	CUfileError_t err = cuFileHandleRegister(&h->cufh, &desc);
	if (err.err != CU_FILE_SUCCESS) {
		free(h);
		return from_cufile_error(err);
	}

	*fh = h;
	use_count++;
	return ds_file_ok();
}

void
ds_file_handle_deregister(ds_file_handle_t fh)
{
	if (!fh)
		return;
	struct gds_handle *h = fh;
	cuFileHandleDeregister(h->cufh);
	free(h);
	use_count--;
}

/* gds resolves layout through cuFile, with no separate filesystem index. */
ds_file_error_t
ds_file_reindex(void)
{
	return ds_file_ok();
}

/* ------------------------------------------------------------------ */
/*  Buffer allocation                                                  */
/* ------------------------------------------------------------------ */

void *
ds_file_alloc(size_t size)
{
	void *ptr;
	if (cudaMalloc(&ptr, size) != cudaSuccess)
		return NULL;
	CUfileError_t err = cuFileBufRegister(ptr, size, 0);
	if (err.err != CU_FILE_SUCCESS) {
		cudaFree(ptr);
		return NULL;
	}
	return ptr;
}

void
ds_file_free(void *buf)
{
	if (!buf)
		return;
	cuFileBufDeregister(buf);
	cudaFree(buf);
}

ds_file_error_t
ds_file_buf_register(const void *buf_base, size_t size, int flags)
{
	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!buf_base || !size)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	CUfileError_t err = cuFileBufRegister(buf_base, size, flags);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_buf_deregister(const void *buf_base)
{
	if (!driver_open)
		return ds_file_err(DS_FILE_DRIVER_NOT_INITIALIZED);
	if (!buf_base)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	CUfileError_t err = cuFileBufDeregister(buf_base);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

/* ------------------------------------------------------------------ */
/*  Synchronous I/O                                                    */
/* ------------------------------------------------------------------ */

ssize_t
ds_file_read(ds_file_handle_t fh, void *buf_base, size_t size,
             off_t file_offset, off_t buf_offset)
{
	struct gds_handle *h = fh;
	return cuFileRead(h->cufh, buf_base, size, file_offset, buf_offset);
}

ssize_t
ds_file_write(ds_file_handle_t fh, const void *buf_base, size_t size,
              off_t file_offset, off_t buf_offset)
{
	struct gds_handle *h = fh;
	return cuFileWrite(h->cufh, buf_base, size, file_offset, buf_offset);
}

/* ------------------------------------------------------------------ */
/*  Batch I/O                                                          */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_batch_io_setup(ds_file_batch_handle_t *batch_idp, unsigned nr)
{
	if (!batch_idp || nr == 0)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	CUfileError_t err =
	        cuFileBatchIOSetUp((CUfileBatchHandle_t *)batch_idp, nr);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_submit(ds_file_batch_handle_t batch_idp, unsigned nr,
                        ds_file_io_params_t *iocbp, unsigned int flags)
{
	if (!batch_idp || !iocbp)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	CUfileIOParams_t *cu_params = calloc(nr, sizeof(*cu_params));
	if (!cu_params)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	for (unsigned i = 0; i < nr; i++) {
		struct gds_handle *h = iocbp[i].fh;
		cu_params[i].mode = (CUfileBatchMode_t)iocbp[i].mode;
		cu_params[i].fh = h->cufh;
		cu_params[i].opcode = (CUfileOpcode_t)iocbp[i].opcode;
		cu_params[i].cookie = iocbp[i].cookie;
		cu_params[i].u.batch.devPtr_base =
		        iocbp[i].u.batch.dev_ptr_base;
		cu_params[i].u.batch.file_offset = iocbp[i].u.batch.file_offset;
		cu_params[i].u.batch.devPtr_offset =
		        iocbp[i].u.batch.dev_ptr_offset;
		cu_params[i].u.batch.size = iocbp[i].u.batch.size;
	}

	CUfileError_t err = cuFileBatchIOSubmit((CUfileBatchHandle_t)batch_idp,
	                                        nr, cu_params, flags);
	free(cu_params);

	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_get_status(ds_file_batch_handle_t batch_idp, unsigned min_nr,
                            unsigned *nr, ds_file_io_events_t *iocbp,
                            struct timespec *timeout)
{
	if (!batch_idp || !nr || !iocbp)
		return ds_file_err(DS_FILE_INVALID_VALUE);

	unsigned max_nr = *nr;
	CUfileIOEvents_t *cu_events = calloc(max_nr, sizeof(*cu_events));
	if (!cu_events)
		return ds_file_err(DS_FILE_INTERNAL_ERROR);

	CUfileError_t err = cuFileBatchIOGetStatus(
	        (CUfileBatchHandle_t)batch_idp, min_nr, nr, cu_events, timeout);

	if (err.err != CU_FILE_SUCCESS) {
		free(cu_events);
		return from_cufile_error(err);
	}

	for (unsigned i = 0; i < *nr; i++) {
		iocbp[i].cookie = cu_events[i].cookie;
		iocbp[i].status = (ds_file_status_t)cu_events[i].status;
		iocbp[i].ret = cu_events[i].ret;
	}

	free(cu_events);
	return ds_file_ok();
}

ds_file_error_t
ds_file_batch_io_cancel(ds_file_batch_handle_t batch_idp)
{
	if (!batch_idp)
		return ds_file_err(DS_FILE_INVALID_VALUE);
	CUfileError_t err = cuFileBatchIOCancel((CUfileBatchHandle_t)batch_idp);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

void
ds_file_batch_io_destroy(ds_file_batch_handle_t batch_idp)
{
	if (!batch_idp)
		return;
	cuFileBatchIODestroy((CUfileBatchHandle_t)batch_idp);
}

/* ------------------------------------------------------------------ */
/*  Async (stream) I/O                                                 */
/* ------------------------------------------------------------------ */

ds_file_error_t
ds_file_read_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                   off_t *file_offset_p, off_t *buf_offset_p,
                   ssize_t *bytes_read_p, ds_stream_t stream)
{
	struct gds_handle *h = fh;
	CUfileError_t err =
	        cuFileReadAsync(h->cufh, buf_base, size_p, file_offset_p,
	                        buf_offset_p, bytes_read_p, (CUstream)stream);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_write_async(ds_file_handle_t fh, void *buf_base, size_t *size_p,
                    off_t *file_offset_p, off_t *buf_offset_p,
                    ssize_t *bytes_written_p, ds_stream_t stream)
{
	struct gds_handle *h = fh;
	CUfileError_t err = cuFileWriteAsync(h->cufh, buf_base, size_p,
	                                     file_offset_p, buf_offset_p,
	                                     bytes_written_p, (CUstream)stream);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_stream_register(ds_stream_t stream, unsigned flags)
{
	CUfileError_t err = cuFileStreamRegister((CUstream)stream, flags);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}

ds_file_error_t
ds_file_stream_deregister(ds_stream_t stream)
{
	CUfileError_t err = cuFileStreamDeregister((CUstream)stream);
	if (err.err != CU_FILE_SUCCESS)
		return from_cufile_error(err);
	return ds_file_ok();
}
