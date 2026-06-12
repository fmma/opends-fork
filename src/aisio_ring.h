#ifndef AISIO_RING_H
#define AISIO_RING_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded lock-free MPSC ring of pointers (SPDK/rte_ring-style, not a linked
 * list). Many caller threads enqueue; one reactor dequeues. A producer reserves
 * a unique slot with a single relaxed fetch_add, then publishes its pointer into
 * that slot; the consumer takes slots in order and clears them. NULL marks a
 * free slot. Capacity is a power of two.
 *
 * Every caller blocks on its op until the reactor completes it, so at most one op
 * per caller is ever in flight: in-flight <= caller-thread count, far below the
 * capacity, so the full-ring producer spin below is effectively never taken. */

#define AISIO_RING_CAP 1024u
#define AISIO_RING_MASK (AISIO_RING_CAP - 1u)

struct aisio_ring {
	_Atomic(void *) slot[AISIO_RING_CAP];
	_Alignas(64) _Atomic uint32_t head; /* producers reserve via fetch_add */
	_Alignas(64) uint32_t tail;         /* consumer-private */
};

static inline void
aisio_ring_pause(void)
{
#if defined(__x86_64__) || defined(__i386__)
	__builtin_ia32_pause();
#endif
}

static inline void
aisio_ring_init(struct aisio_ring *q)
{
	for (uint32_t i = 0; i < AISIO_RING_CAP; i++)
		atomic_store_explicit(&q->slot[i], NULL, memory_order_relaxed);
	atomic_store_explicit(&q->head, 0, memory_order_relaxed);
	q->tail = 0;
}

/* Producer (many threads). Reserve a unique position, wait for its slot to drain
 * (only if the ring is full), then publish. The release store hands the pointer
 * -- and the payload it points at -- to the consumer's acquire load. */
static inline void
aisio_ring_enqueue(struct aisio_ring *q, void *item)
{
	uint32_t pos = atomic_fetch_add_explicit(&q->head, 1, memory_order_relaxed);
	_Atomic(void *) *slot = &q->slot[pos & AISIO_RING_MASK];
	while (atomic_load_explicit(slot, memory_order_acquire) != NULL)
		aisio_ring_pause();
	atomic_store_explicit(slot, item, memory_order_release);
}

/* Consumer (single reactor). Returns NULL when the tail slot is empty -- the
 * ring is empty, or the slot's producer reserved its position but hasn't
 * published yet; the reactor just polls again. */
static inline void *
aisio_ring_dequeue(struct aisio_ring *q)
{
	_Atomic(void *) *slot = &q->slot[q->tail & AISIO_RING_MASK];
	void *item = atomic_load_explicit(slot, memory_order_acquire);
	if (!item)
		return NULL;
	atomic_store_explicit(slot, NULL, memory_order_release);
	q->tail++;
	return item;
}

#endif /* AISIO_RING_H */
