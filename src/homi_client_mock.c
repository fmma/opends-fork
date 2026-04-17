#define _GNU_SOURCE

#include "homi_client_mock.h"
#include "extent_cache.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct homi_conn {
	struct homi_extent *cached_extents;
	uint32_t cached_count;
	char cached_bdf[16];
};

static int
load_extent_cache(struct homi_conn *c, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -errno;

	struct extent_cache_header hdr;
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		fclose(f);
		return -EINVAL;
	}

	c->cached_extents =
	        calloc(hdr.extent_count, sizeof(*c->cached_extents));
	if (!c->cached_extents) {
		fclose(f);
		return -ENOMEM;
	}

	if (fread(c->cached_extents, sizeof(*c->cached_extents),
	          hdr.extent_count, f) != hdr.extent_count) {
		free(c->cached_extents);
		c->cached_extents = NULL;
		fclose(f);
		return -EIO;
	}

	c->cached_count = hdr.extent_count;
	memcpy(c->cached_bdf, hdr.bdf, sizeof(c->cached_bdf));
	fclose(f);
	return 0;
}

int
homi_connect(const char *shm_name, struct homi_conn **conn)
{
	(void)shm_name;

	const char *cache_path = getenv("OPENDS_EXTENT_CACHE");
	if (!cache_path)
		return -EINVAL;

	struct homi_conn *c = calloc(1, sizeof(*c));
	if (!c)
		return -ENOMEM;

	int rc = load_extent_cache(c, cache_path);
	if (rc < 0) {
		free(c);
		return rc;
	}

	*conn = c;
	return 0;
}

void
homi_disconnect(struct homi_conn *conn)
{
	if (!conn)
		return;
	free(conn->cached_extents);
	free(conn);
}

int
homi_get_extents(struct homi_conn *conn, int fd, struct homi_extent_list **out)
{
	(void)fd;

	struct homi_extent_list *list = calloc(1, sizeof(*list));
	if (!list)
		return -ENOMEM;

	list->extents = calloc(conn->cached_count, sizeof(*list->extents));
	if (!list->extents) {
		free(list);
		return -ENOMEM;
	}

	memcpy(list->extents, conn->cached_extents,
	       conn->cached_count * sizeof(*list->extents));
	list->count = conn->cached_count;
	*out = list;
	return 0;
}

void
homi_extent_list_free(struct homi_extent_list *list)
{
	if (!list)
		return;
	free(list->extents);
	free(list);
}

int
homi_get_device_uri(struct homi_conn *conn, int fd, char **uri)
{
	(void)fd;

	*uri = strdup(conn->cached_bdf);
	if (!*uri)
		return -ENOMEM;
	return 0;
}
