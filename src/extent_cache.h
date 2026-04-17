/*
 * extent_cache.h - On-disk format for the HOMI mock extent cache.
 *
 * Produced by tools/cache_extents, consumed by homi_client_mock. The
 * header is followed by header.extent_count extent_cache_record
 * entries. Length is stored in bytes (not LBAs) so the consumer can
 * reject requests that overlap a partial trailing LBA past EOF.
 */
#ifndef EXTENT_CACHE_H_
#define EXTENT_CACHE_H_

#include <stdint.h>

struct extent_cache_header {
	uint32_t extent_count;
	char bdf[16];
};

struct extent_cache_record {
	uint64_t file_offset;
	uint64_t slba;
	uint64_t length;
};

#endif /* EXTENT_CACHE_H_ */
