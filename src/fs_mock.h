/*
 * fs_mock.h - File-system layer mock for the aisio backend.
 *
 * Stands in for the kernel FS layer in tests and benchmarks. Loads a
 * multi-file extent cache produced by tools/cache_extents (FIEMAP walk
 * over a mounted dataset) and resolves absolute paths to stable mock
 * file handles. The aisio backend's HOMI mock delegates extent and
 * device-URI lookups here.
 *
 * Transitional consumer interface: installed alongside the aisio
 * library so external benchmarks can drive the backend until real
 * HOMI lands. Goes away once HOMI replaces the mock.
 */
#ifndef FS_MOCK_H_
#define FS_MOCK_H_

#include "homi_types.h"

#include <stdint.h>

/* Load the cache file at `cache_path`, replacing any prior state.
 * Returns 0 on success or -errno on failure. */
int fs_mock_init(const char *cache_path);

/* Resolve `path` to a stable mock fh (>= 0), or -ENOENT if the path
 * is not present in the loaded cache. The returned fh is suitable as
 * the fd argument to ds_file_handle_register. */
int fs_mock_open(const char *path);

/* Used by homi_client_mock to satisfy homi_get_extents/_get_device_uri.
 * The returned `extents` and `uri` pointers reference state owned by
 * fs_mock and are valid until fs_mock_reset. */
int fs_mock_get_extents(int fh, const struct homi_extent **extents,
                        uint32_t *count);
int fs_mock_get_device_uri(int fh, const char **uri);

/* Path-table enumeration. Lets consumers walk the dataset without
 * needing to mount the filesystem or know paths up front. The
 * indices match those returned by fs_mock_open for the same path,
 * so a consumer can iterate i < fs_mock_get_n_files() and pass i
 * directly to ds_file_handle_register. */
uint32_t fs_mock_get_n_files(void);
int fs_mock_get_path(uint32_t i, const char **path);

/* Logical file size derived from the last extent's file_offset +
 * length. The cache does not store the original stat size, so
 * tail bytes past the last extent (sparse holes) are not represented. */
int fs_mock_get_size(uint32_t i, uint64_t *size);

/* Free all loaded state. */
void fs_mock_reset(void);

#endif /* FS_MOCK_H_ */
