/*
 * homi_client_mock.h - Mock HOMI protocol client.
 *
 * HOMI (Hyperscale Object-storage Metadata Interface) will provide
 * file-to-physical-extent mapping and device metadata over a shared
 * memory channel. This mock stands in for that service by loading a
 * pre-generated cache file (see tools/cache_extents).
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

struct homi_extent_list {
	struct homi_extent *extents;
	uint32_t count;
};

struct homi_conn;

int homi_connect(const char *shm_name, struct homi_conn **conn);
void homi_disconnect(struct homi_conn *conn);

/* Caller frees with homi_extent_list_free. */
int homi_get_extents(struct homi_conn *conn, int fd,
                     struct homi_extent_list **out);
void homi_extent_list_free(struct homi_extent_list *list);

/* Returned string is owned by the caller. */
int homi_get_device_uri(struct homi_conn *conn, int fd, char **uri);

#endif /* HOMI_CLIENT_MOCK_H_ */
