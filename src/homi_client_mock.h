/*
 * homi_client_mock.h - Mock HOMI protocol client.
 *
 * HOMI (Hyperscale Object-storage Metadata Interface) will provide
 * file-to-physical-extent mapping and device metadata over a shared
 * memory channel. This mock stands in for that service by delegating
 * lookups to fs_mock, which holds the loaded extent cache.
 *
 * Internal to the aisio backend. Not part of the public API.
 */
#ifndef HOMI_CLIENT_MOCK_H_
#define HOMI_CLIENT_MOCK_H_

#include <stddef.h>
#include <stdint.h>

struct homi_extent {
	uint64_t file_offset;
	uint64_t slba;
	uint64_t length;
};

struct homi_conn;

int homi_connect(const char *shm_name, struct homi_conn **conn);
void homi_disconnect(struct homi_conn *conn);

/* Borrowed pointer into fs_mock's loaded extents. Valid until
 * fs_mock_reset. Caller must not free or mutate. */
int homi_get_extents(struct homi_conn *conn, int fd,
                     const struct homi_extent **extents, uint32_t *count);

/* Returned string is owned by the caller. */
int homi_get_device_uri(struct homi_conn *conn, int fd, char **uri);

#endif /* HOMI_CLIENT_MOCK_H_ */
