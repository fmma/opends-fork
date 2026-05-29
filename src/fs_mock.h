/*
 * fs_mock.h - File-system layer mock for the aisio backend.
 *
 * Stands in for HOMI in tests and benchmarks. Holds an in-memory
 * (device_uri, registered files) state and answers homi_get_extents
 * and homi_get_device_uri lookups out of it.
 *
 * Transitional consumer interface: installed alongside the aisio
 * library so external benchmarks can drive the mock until real HOMI
 * lands. Goes away once HOMI replaces the mock.
 */
#ifndef FS_MOCK_H_
#define FS_MOCK_H_

#include "homi_types.h"

#include <stdint.h>

/* Set the device URI returned by fs_mock_get_device_uri for every
 * subsequently registered file, and clear any prior registrations.
 * Returns 0 on success or -EINVAL if uri is NULL/too long. */
int fs_mock_init(const char *device_uri);

/* Register an extent list with its logical file size and return a
 * stable mock fh (>= 0) suitable as the fd argument to
 * ds_file_handle_register. The caller's extent array is copied;
 * subsequent fs_mock state changes do not invalidate pointers returned
 * via fs_mock_get_extents. The optional `path` is copied and made
 * available via fs_mock_get_path; pass NULL when no path is known.
 * Returns -ENOMEM on allocation failure or -EINVAL if extents is NULL
 * with count > 0. */
int fs_mock_register(const struct homi_extent *extents, uint32_t count,
                     uint64_t size, const char *path);

/* Used by homi_client_mock to satisfy homi_get_extents/_get_device_uri.
 * The returned `extents` and `uri` pointers reference state owned by
 * fs_mock and are valid until fs_mock_reset. */
int fs_mock_get_extents(int fh, const struct homi_extent **extents,
                        uint32_t *count);
int fs_mock_get_device_uri(int fh, const char **uri);

/* Return the logical file size registered alongside the extents.
 * Returns -ENOENT if fh is out of range. */
int fs_mock_get_size(int fh, uint64_t *size);

/* Return the path that was passed to fs_mock_register, or NULL if no
 * path was registered. Returns -ENOENT if fh is out of range. */
int fs_mock_get_path(int fh, const char **path);

/* Free all registered state. */
void fs_mock_reset(void);

#endif /* FS_MOCK_H_ */
