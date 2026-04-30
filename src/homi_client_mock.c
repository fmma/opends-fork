#define _GNU_SOURCE

#include "homi_client_mock.h"
#include "fs_mock.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct homi_conn {
	int sentinel;
};

int
homi_connect(const char *shm_name, struct homi_conn **conn)
{
	(void)shm_name;

	struct homi_conn *c = calloc(1, sizeof(*c));
	if (!c)
		return -ENOMEM;

	*conn = c;
	return 0;
}

void
homi_disconnect(struct homi_conn *conn)
{
	free(conn);
}

int
homi_get_extents(struct homi_conn *conn, int fd,
                 const struct homi_extent **extents, uint32_t *count)
{
	(void)conn;
	return fs_mock_get_extents(fd, extents, count);
}

int
homi_get_device_uri(struct homi_conn *conn, int fd, char **uri)
{
	(void)conn;

	const char *src = NULL;
	int rc = fs_mock_get_device_uri(fd, &src);
	if (rc < 0)
		return rc;

	*uri = strdup(src);
	return *uri ? 0 : -ENOMEM;
}
