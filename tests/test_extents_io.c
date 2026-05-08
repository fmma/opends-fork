#define _GNU_SOURCE

#include "test_extents_io.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#define MAX_EXTENTS 1024
#define SECTOR_SIZE 512

static int
resolve_nvme_ns_name(int fd, char *ns_name, size_t len)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return -errno;

	char sys_path[256];
	char link_target[256];
	snprintf(sys_path, sizeof(sys_path), "/sys/dev/block/%u:%u",
	         major(st.st_dev), minor(st.st_dev));

	ssize_t n = readlink(sys_path, link_target, sizeof(link_target) - 1);
	if (n <= 0)
		return -ENOENT;
	link_target[n] = '\0';

	char *base = strrchr(link_target, '/');
	if (!base)
		return -ENOENT;
	base++;

	strncpy(ns_name, base, len - 1);
	ns_name[len - 1] = '\0';

	if (strncmp(ns_name, "nvme", 4) != 0)
		return -ENOTSUP;

	/* Strip partition suffix (e.g. nvme0n1p1 -> nvme0n1). */
	char *p = strstr(ns_name + 4, "p");
	if (p && p > ns_name + 4 && *(p - 1) >= '0' && *(p - 1) <= '9' &&
	    *(p + 1) >= '0' && *(p + 1) <= '9')
		*p = '\0';

	return 0;
}

static uint64_t
get_partition_start_bytes(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return 0;

	char sys_path[256];
	char link_target[256];
	snprintf(sys_path, sizeof(sys_path), "/sys/dev/block/%u:%u",
	         major(st.st_dev), minor(st.st_dev));

	ssize_t n = readlink(sys_path, link_target, sizeof(link_target) - 1);
	if (n <= 0)
		return 0;
	link_target[n] = '\0';

	char *base = strrchr(link_target, '/');
	if (!base)
		return 0;
	base++;

	char start_path[320];
	snprintf(start_path, sizeof(start_path), "/sys/class/block/%s/start",
	         base);

	FILE *f = fopen(start_path, "r");
	if (!f)
		return 0;

	uint64_t start_sectors = 0;
	if (fscanf(f, "%lu", &start_sectors) != 1)
		start_sectors = 0;
	fclose(f);

	return start_sectors * 512;
}

static uint32_t
get_lba_size(int fd)
{
	char ns_name[64];
	if (resolve_nvme_ns_name(fd, ns_name, sizeof(ns_name)) < 0)
		return SECTOR_SIZE;

	char lbs_path[256];
	snprintf(lbs_path, sizeof(lbs_path),
	         "/sys/block/%s/queue/logical_block_size", ns_name);

	uint32_t lba_size = SECTOR_SIZE;
	FILE *f = fopen(lbs_path, "r");
	if (f) {
		int val = 0;
		if (fscanf(f, "%d", &val) == 1 && val > 0)
			lba_size = (uint32_t)val;
		fclose(f);
	}

	return lba_size;
}

static int
resolve_pci_bdf(int fd, char *bdf, size_t len)
{
	char ns_name[64];
	int rc = resolve_nvme_ns_name(fd, ns_name, sizeof(ns_name));
	if (rc < 0)
		return rc;

	char dev_path[256];
	char link_target[256];
	snprintf(dev_path, sizeof(dev_path), "/sys/block/%s/device/device",
	         ns_name);

	ssize_t n = readlink(dev_path, link_target, sizeof(link_target) - 1);
	if (n <= 0) {
		snprintf(dev_path, sizeof(dev_path), "/sys/block/%s/device",
		         ns_name);
		n = readlink(dev_path, link_target, sizeof(link_target) - 1);
		if (n <= 0)
			return -ENOENT;
	}
	link_target[n] = '\0';

	char *base = strrchr(link_target, '/');
	if (!base)
		return -ENOENT;
	base++;

	if (!strchr(base, ':'))
		return -ENOENT;

	strncpy(bdf, base, len - 1);
	bdf[len - 1] = '\0';
	return 0;
}

static int
fiemap_file(int fd, uint64_t file_size, uint32_t lba_size,
            uint64_t part_start,
            struct homi_extent **out_extents, uint32_t *out_count)
{
	size_t buf_size = sizeof(struct fiemap) +
	                  MAX_EXTENTS * sizeof(struct fiemap_extent);
	struct fiemap *fm = calloc(1, buf_size);
	if (!fm)
		return -ENOMEM;

	fm->fm_start = 0;
	fm->fm_length = FIEMAP_MAX_OFFSET;
	fm->fm_flags = 0;
	fm->fm_extent_count = MAX_EXTENTS;

	if (ioctl(fd, FS_IOC_FIEMAP, fm) < 0) {
		int rc = -errno;
		free(fm);
		return rc;
	}

	uint32_t count = fm->fm_mapped_extents;
	if (count == 0) {
		free(fm);
		*out_extents = NULL;
		*out_count = 0;
		return 0;
	}

	if (!(fm->fm_extents[count - 1].fe_flags & FIEMAP_EXTENT_LAST)) {
		fprintf(stderr,
		        "extent list truncated at %u entries (MAX_EXTENTS=%d); "
		        "raise MAX_EXTENTS and rebuild\n", count, MAX_EXTENTS);
		free(fm);
		return -E2BIG;
	}

	struct homi_extent *ext = calloc(count, sizeof(*ext));
	if (!ext) {
		free(fm);
		return -ENOMEM;
	}

	for (uint32_t i = 0; i < count; i++) {
		struct fiemap_extent *fe = &fm->fm_extents[i];
		ext[i].file_offset = fe->fe_logical;
		ext[i].slba = (fe->fe_physical + part_start) / lba_size;
		ext[i].length = fe->fe_length;

		uint64_t end = ext[i].file_offset + ext[i].length;
		if (end > file_size)
			ext[i].length = file_size - ext[i].file_offset;
	}

	free(fm);
	*out_extents = ext;
	*out_count = count;
	return 0;
}

int
test_extents_emit(const char *fs_path, const char *out_path)
{
	int fd = open(fs_path, O_RDONLY);
	if (fd < 0)
		return -errno;

	struct stat st;
	if (fstat(fd, &st) < 0) {
		int rc = -errno;
		close(fd);
		return rc;
	}

	struct test_extents_header hdr = {
	        .magic = TEST_EXTENTS_MAGIC,
	        .version = TEST_EXTENTS_VERSION,
	};
	if (resolve_pci_bdf(fd, hdr.uri, sizeof(hdr.uri)) < 0) {
		fprintf(stderr, "resolve_pci_bdf(%s) failed\n", fs_path);
		close(fd);
		return -ENOENT;
	}
	uint32_t lba_size = get_lba_size(fd);
	uint64_t part_start = get_partition_start_bytes(fd);

	struct homi_extent *extents = NULL;
	uint32_t n_extents = 0;
	int rc = fiemap_file(fd, (uint64_t)st.st_size, lba_size, part_start,
	                     &extents, &n_extents);
	close(fd);
	if (rc < 0)
		return rc;

	hdr.n_extents = n_extents;

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		rc = -errno;
		free(extents);
		return rc;
	}
	if (fwrite(&hdr, sizeof(hdr), 1, out) != 1 ||
	    (n_extents > 0 &&
	     fwrite(extents, sizeof(*extents), n_extents, out) != n_extents)) {
		rc = -EIO;
	}
	fclose(out);
	free(extents);
	return rc;
}

int
test_extents_load(const char *path, char uri[64],
                  struct homi_extent **extents, uint32_t *n_extents)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -errno;

	struct test_extents_header hdr;
	if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
		fclose(f);
		return -EIO;
	}
	if (hdr.magic != TEST_EXTENTS_MAGIC ||
	    hdr.version != TEST_EXTENTS_VERSION) {
		fclose(f);
		return -EINVAL;
	}

	memcpy(uri, hdr.uri, 64);

	struct homi_extent *e = NULL;
	if (hdr.n_extents > 0) {
		e = malloc(hdr.n_extents * sizeof(*e));
		if (!e) {
			fclose(f);
			return -ENOMEM;
		}
		if (fread(e, sizeof(*e), hdr.n_extents, f) != hdr.n_extents) {
			free(e);
			fclose(f);
			return -EIO;
		}
	}

	fclose(f);
	*extents = e;
	*n_extents = hdr.n_extents;
	return 0;
}
