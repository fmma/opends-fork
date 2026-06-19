/*
 * cu_stream_map.h - tiny open-addressing map keyed on CUstream.
 *
 * Backs opends_aisio's CUstream -> stream_state lookup. The driver
 * registers up to MAX_STREAMS streams and then performs an
 * O(1) lookup per opends_read_async call. Grow-only: there is no
 * delete primitive, so the table needs no tombstones.
 *
 * Caller owns the entry array (typically inline in the driver struct).
 * Capacity must be a power of two; pass mask = capacity - 1. Load
 * factor stays below 50% if capacity >= 2 * MAX_STREAMS.
 *
 * The value is a small int (typically an index into the driver's
 * streams[] array). NULL key means empty slot; CUDA never returns
 * NULL from cuStreamCreate and the pseudo-streams (CU_STREAM_LEGACY
 * = 0x1, CU_STREAM_PER_THREAD = 0x2) are nonzero, so NULL is safe as
 * the empty sentinel.
 */

#ifndef OPENDS_CU_STREAM_MAP_H
#define OPENDS_CU_STREAM_MAP_H

#include <cuda.h>
#include <stdint.h>

struct cu_stream_map_entry {
	CUstream key;
	int idx;
};

static inline uint32_t
cu_stream_map_hash(CUstream cus, uint32_t mask)
{
	uintptr_t v = (uintptr_t)cus;
	v *= 0x9E3779B97F4A7C15ULL; /* Fibonacci hash */
	return (uint32_t)(v >> 32) & mask;
}

/* Returns idx on hit, -1 on miss. Lock-free against concurrent
 * cu_stream_map_put on a different key: a reader for K1 only walks
 * slots [hash(K1), K1's_slot], and those were all non-empty at K1's
 * registration (else K1 would have stopped sooner). The table is
 * grow-only, so a slot in that range cannot later become empty or
 * be the destination of another concurrent insert. */
static inline int
cu_stream_map_get(const struct cu_stream_map_entry *e, uint32_t mask,
                  CUstream stream)
{
	uint32_t h = cu_stream_map_hash(stream, mask);
	uint32_t cap = mask + 1;
	for (uint32_t p = 0; p < cap; p++) {
		const struct cu_stream_map_entry *s = &e[(h + p) & mask];
		if (s->key == stream)
			return s->idx;
		if (s->key == NULL)
			return -1;
	}
	return -1;
}

/* Insert, returning 0 on success and -1 if full. Caller must
 * serialize inserts (in opends_aisio.c, alloc_mtx) and keep the load
 * factor below 50%. See cu_stream_map_get for the lock-free reader
 * invariant. */
static inline int
cu_stream_map_put(struct cu_stream_map_entry *e, uint32_t mask, CUstream stream,
                  int idx)
{
	uint32_t h = cu_stream_map_hash(stream, mask);
	uint32_t cap = mask + 1;
	for (uint32_t p = 0; p < cap; p++) {
		struct cu_stream_map_entry *s = &e[(h + p) & mask];
		if (s->key == NULL) {
			s->idx = idx;
			s->key = stream;
			return 0;
		}
	}
	return -1;
}

#endif /* OPENDS_CU_STREAM_MAP_H */
