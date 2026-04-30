#define _GNU_SOURCE

#include "fs_mock.h"
#include "extent_cache.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct fs_mock_entry {
	char *path;
	struct homi_extent *extents;
	uint32_t n_extents;
};

static struct {
	struct fs_mock_entry *entries;
	uint32_t n_entries;
	char bdf[16];
} state;

static int
load_file_record(FILE *f, struct fs_mock_entry *e)
{
	struct extent_cache_file_record fr;
	if (fread(&fr, sizeof(fr), 1, f) != 1)
		return -EIO;
	if (fr.path_len == 0)
		return -EINVAL;

	e->path = malloc(fr.path_len);
	if (!e->path)
		return -ENOMEM;
	if (fread(e->path, 1, fr.path_len, f) != fr.path_len)
		return -EIO;
	e->path[fr.path_len - 1] = '\0';

	e->n_extents = fr.n_extents;
	if (fr.n_extents == 0)
		return 0;

	e->extents = calloc(fr.n_extents, sizeof(*e->extents));
	if (!e->extents)
		return -ENOMEM;

	for (uint32_t i = 0; i < fr.n_extents; i++) {
		struct extent_cache_extent rec;
		if (fread(&rec, sizeof(rec), 1, f) != 1)
			return -EIO;
		e->extents[i].file_offset = rec.file_offset;
		e->extents[i].slba = rec.slba;
		e->extents[i].length = rec.length;
	}
	return 0;
}

int
fs_mock_init(const char *cache_path)
{
	fs_mock_reset();

	FILE *f = fopen(cache_path, "rb");
	if (!f)
		return -errno;

	struct extent_cache_header hdr;
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		fclose(f);
		return -EIO;
	}
	if (hdr.magic != EXTENT_CACHE_MAGIC ||
	    hdr.version != EXTENT_CACHE_VERSION) {
		fclose(f);
		return -EINVAL;
	}

	state.entries = calloc(hdr.n_files, sizeof(*state.entries));
	if (!state.entries) {
		fclose(f);
		return -ENOMEM;
	}
	state.n_entries = hdr.n_files;
	memcpy(state.bdf, hdr.bdf, sizeof(state.bdf));

	for (uint32_t i = 0; i < hdr.n_files; i++) {
		int rc = load_file_record(f, &state.entries[i]);
		if (rc < 0) {
			fclose(f);
			fs_mock_reset();
			return rc;
		}
	}

	fclose(f);
	return 0;
}

int
fs_mock_open(const char *path)
{
	if (!path)
		return -EINVAL;
	for (uint32_t i = 0; i < state.n_entries; i++) {
		if (strcmp(state.entries[i].path, path) == 0)
			return (int)i;
	}
	return -ENOENT;
}

int
fs_mock_get_extents(int fh, const struct homi_extent **extents,
                    uint32_t *count)
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
	*uri = state.bdf;
	return 0;
}

void
fs_mock_reset(void)
{
	for (uint32_t i = 0; i < state.n_entries; i++) {
		free(state.entries[i].path);
		free(state.entries[i].extents);
	}
	free(state.entries);
	state.entries = NULL;
	state.n_entries = 0;
	memset(state.bdf, 0, sizeof(state.bdf));
}
