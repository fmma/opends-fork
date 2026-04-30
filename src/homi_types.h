/*
 * homi_types.h - Shared HOMI types.
 *
 * Standalone header so lower-level modules (fs_mock, extent_cache)
 * can depend on these types without pulling in homi_client_mock.h.
 */
#ifndef HOMI_TYPES_H_
#define HOMI_TYPES_H_

#include <stdint.h>

struct homi_extent {
	uint64_t file_offset;
	uint64_t slba;
	uint64_t length;
};

#endif /* HOMI_TYPES_H_ */
