/* Host-only unit test for the reactor inbound ring (src/aisio_ring.h): many
 * producers enqueue tagged messages, one consumer drains, and every (producer,
 * seq) must arrive exactly once. The producer count exceeds the consumer's
 * drain rate, so the bounded ring also exercises the full/backpressure path. No
 * NVMe or CUDA, so it always builds and runs fast under the normal test harness. */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "aisio_ring.h"

#define PRODUCERS 8
#define PER_PRODUCER 50000
#define ROUNDS 3

struct msg {
	int producer;
	int seq;
};

static struct aisio_ring q;
static struct msg *pool[PRODUCERS];
static _Atomic int go;

static void *
producer(void *arg)
{
	long id = (long)arg;
	while (atomic_load(&go) == 0)
		;
	for (int i = 0; i < PER_PRODUCER; i++) {
		pool[id][i].producer = (int)id;
		pool[id][i].seq = i;
		aisio_ring_enqueue(&q, &pool[id][i]);
	}
	return NULL;
}

static int
run_round(void)
{
	static char seen[PRODUCERS][PER_PRODUCER];
	pthread_t th[PRODUCERS];
	long got = 0, dups = 0, missing = 0;

	for (int p = 0; p < PRODUCERS; p++)
		for (int s = 0; s < PER_PRODUCER; s++)
			seen[p][s] = 0;

	aisio_ring_init(&q);
	atomic_store(&go, 0);
	for (long i = 0; i < PRODUCERS; i++)
		pthread_create(&th[i], NULL, producer, (void *)i);
	atomic_store(&go, 1);

	while (got < (long)PRODUCERS * PER_PRODUCER) {
		struct msg *m = aisio_ring_dequeue(&q);
		if (!m)
			continue;
		if (m->producer < 0 || m->producer >= PRODUCERS || m->seq < 0 ||
		    m->seq >= PER_PRODUCER) {
			fprintf(stderr, "FAIL: garbage msg p=%d s=%d\n", m->producer, m->seq);
			return 1;
		}
		if (seen[m->producer][m->seq]++)
			dups++;
		got++;
	}

	for (int i = 0; i < PRODUCERS; i++)
		pthread_join(th[i], NULL);
	while (aisio_ring_dequeue(&q))
		;

	for (int p = 0; p < PRODUCERS; p++)
		for (int s = 0; s < PER_PRODUCER; s++)
			if (!seen[p][s])
				missing++;

	if (got != (long)PRODUCERS * PER_PRODUCER || dups || missing) {
		fprintf(stderr, "FAIL: got=%ld expected=%d dups=%ld missing=%ld\n", got,
			PRODUCERS * PER_PRODUCER, dups, missing);
		return 1;
	}
	return 0;
}

int
main(void)
{
	for (int i = 0; i < PRODUCERS; i++) {
		pool[i] = calloc(PER_PRODUCER, sizeof(struct msg));
		if (!pool[i]) {
			fprintf(stderr, "FAIL: alloc\n");
			return 1;
		}
	}
	for (int r = 0; r < ROUNDS; r++) {
		if (run_round() != 0)
			return 1;
	}
	for (int i = 0; i < PRODUCERS; i++)
		free(pool[i]);
	printf("ring: %d rounds x %d producers x %d items PASS\n", ROUNDS, PRODUCERS,
	       PER_PRODUCER);
	return 0;
}
