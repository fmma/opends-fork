#define _GNU_SOURCE

#include "fs_mock.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct fs_mock_entry {
	struct homi_extent *extents;
	uint32_t n_extents;
	uint64_t size;
	char *path;
};

static struct {
	struct fs_mock_entry *entries;
	uint32_t n_entries;
	uint32_t cap;
	char uri[64];
} state;

int
fs_mock_init(const char *device_uri)
{
	if (!device_uri)
		return -EINVAL;
	if (strlen(device_uri) >= sizeof(state.uri))
		return -EINVAL;

	fs_mock_reset();
	strcpy(state.uri, device_uri);
	return 0;
}

int
fs_mock_register(const struct homi_extent *extents, uint32_t count, uint64_t size,
                 const char *path)
{
	if (count > 0 && !extents)
		return -EINVAL;

	if (state.n_entries == state.cap) {
		uint32_t new_cap = state.cap ? state.cap * 2 : 64;
		struct fs_mock_entry *grown =
		        realloc(state.entries, new_cap * sizeof(*grown));
		if (!grown)
			return -ENOMEM;
		state.entries = grown;
		state.cap = new_cap;
	}

	struct fs_mock_entry *e = &state.entries[state.n_entries];
	e->extents = NULL;
	e->n_extents = count;
	e->size = size;
	e->path = NULL;
	if (count > 0) {
		e->extents = malloc(count * sizeof(*e->extents));
		if (!e->extents)
			return -ENOMEM;
		memcpy(e->extents, extents, count * sizeof(*e->extents));
	}
	if (path) {
		e->path = strdup(path);
		if (!e->path) {
			free(e->extents);
			e->extents = NULL;
			return -ENOMEM;
		}
	}

	return (int)state.n_entries++;
}

int
fs_mock_get_extents(int fh, const struct homi_extent **extents, uint32_t *count)
{
	if (fh < 0 || (uint32_t)fh >= state.n_entries)
		return -ENOENT;
	*extents = state.entries[fh].extents;
	*count = state.entries[fh].n_extents;
	return 0;
}

int
fs_mock_get_device_uri(int fh, const char **uri)
{
	if (fh < 0 || (uint32_t)fh >= state.n_entries)
		return -ENOENT;
	*uri = state.uri;
	return 0;
}

int
fs_mock_get_size(int fh, uint64_t *size)
{
	if (fh < 0 || (uint32_t)fh >= state.n_entries)
		return -ENOENT;
	*size = state.entries[fh].size;
	return 0;
}

int
fs_mock_get_path(int fh, const char **path)
{
	if (fh < 0 || (uint32_t)fh >= state.n_entries)
		return -ENOENT;
	*path = state.entries[fh].path;
	return 0;
}

void
fs_mock_reset(void)
{
	for (uint32_t i = 0; i < state.n_entries; i++) {
		free(state.entries[i].extents);
		free(state.entries[i].path);
	}
	free(state.entries);
	state.entries = NULL;
	state.n_entries = 0;
	state.cap = 0;
	memset(state.uri, 0, sizeof(state.uri));
}
