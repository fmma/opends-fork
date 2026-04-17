/*
 * extent_cache.h - On-disk format for the HOMI mock extent cache.
 *
 * Produced by tools/cache_extents, consumed by homi_client_mock. The
 * header is followed by extent_count * struct homi_extent records.
 */
#ifndef EXTENT_CACHE_H_
#define EXTENT_CACHE_H_

#include <stdint.h>

struct extent_cache_header {
	uint32_t lba_size;
	uint32_t extent_count;
	char bdf[16];
};

#endif /* EXTENT_CACHE_H_ */
