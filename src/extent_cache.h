/*
 * extent_cache.h - On-disk format for the HOMI mock extent cache.
 *
 * Produced by tools/cache_extents (FIEMAP walk over a mounted dataset)
 * and consumed by src/fs_mock.c. Holds physical extents for many files
 * sharing one NVMe namespace, keyed by absolute path.
 *
 * Layout:
 *   extent_cache_header
 *   for each file:
 *     extent_cache_file_record
 *     path[path_len]                      (null-terminated)
 *     extent_cache_extent[n_extents]
 *
 * Lengths are bytes (not LBAs) so the consumer can reject requests
 * that overlap partial trailing LBAs past EOF.
 */
#ifndef EXTENT_CACHE_H_
#define EXTENT_CACHE_H_

#include <stdint.h>

#define EXTENT_CACHE_MAGIC   0x53504F44u  /* "DOPS" little-endian */
#define EXTENT_CACHE_VERSION 2u

struct extent_cache_header {
	uint32_t magic;
	uint32_t version;
	char     bdf[16];
	uint32_t n_files;
};

struct extent_cache_file_record {
	uint32_t path_len;   /* bytes, including the trailing null */
	uint32_t n_extents;
};

struct extent_cache_extent {
	uint64_t file_offset;
	uint64_t slba;
	uint64_t length;
};

#endif /* EXTENT_CACHE_H_ */
