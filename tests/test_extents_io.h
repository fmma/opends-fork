/*
 * test_extents_io.h - Tiny on-disk extent record for the aisio tests.
 *
 * Captures one file's extents plus the device URI so the aisio tests
 * (which run after the kernel nvme driver has been unbound from the
 * device) can populate fs_mock without re-FIEMAPing the now-unmounted
 * filesystem. Produced by test_sync_read_prep while the FS is still
 * mounted.
 *
 * Format is private to the test suite and is not part of OpenDS' API.
 */
#ifndef TEST_EXTENTS_IO_H_
#define TEST_EXTENTS_IO_H_

#include "homi_types.h"

#include <stdint.h>

#define TEST_EXTENTS_MAGIC 0x53455453u /* "STES" little-endian */
#define TEST_EXTENTS_VERSION 1u

struct test_extents_header {
	uint32_t magic;
	uint32_t version;
	char uri[64];
	uint32_t n_extents;
	uint32_t reserved;
};

/* FIEMAP `fs_path`, resolve its containing NVMe device's PCI BDF, and
 * write the resulting (uri, extents) record to `out_path`. Returns 0
 * on success or -errno on failure. Requires `fs_path` to be on a
 * mounted XFS-on-NVMe namespace or partition. */
int test_extents_emit(const char *fs_path, const char *out_path);

/* Read a record produced by test_extents_emit. The returned
 * `extents` array is malloc'd; the caller frees it. */
int test_extents_load(const char *path, char uri[64],
                      struct homi_extent **extents, uint32_t *n_extents);

#endif /* TEST_EXTENTS_IO_H_ */
