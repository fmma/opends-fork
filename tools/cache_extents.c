/*
 * cache_extents - Build the HOMI mock extent cache for a file.
 *
 * Runs FS_IOC_FIEMAP to extract the file's physical extents, resolves
 * the backing NVMe namespace's PCI BDF, and writes the result to disk
 * in the extent_cache format consumed by the aisio backend's mock
 * HOMI client.
 */

#define _GNU_SOURCE

#include "extent_cache.h"

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

int
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <input_file> <output.bin>\n",
		        argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	struct stat file_st;
	if (fstat(fd, &file_st) < 0) {
		perror("fstat");
		close(fd);
		return 1;
	}
	uint64_t file_size = (uint64_t)file_st.st_size;

	uint32_t lba_size = get_lba_size(fd);

	char bdf[16] = {0};
	if (resolve_pci_bdf(fd, bdf, sizeof(bdf)) < 0)
		fprintf(stderr, "warning: could not resolve PCI BDF\n");

	size_t buf_size = sizeof(struct fiemap) +
	                  MAX_EXTENTS * sizeof(struct fiemap_extent);
	struct fiemap *fm = calloc(1, buf_size);
	if (!fm) {
		perror("calloc");
		close(fd);
		return 1;
	}

	fm->fm_start = 0;
	fm->fm_length = FIEMAP_MAX_OFFSET;
	fm->fm_flags = 0;
	fm->fm_extent_count = MAX_EXTENTS;

	if (ioctl(fd, FS_IOC_FIEMAP, fm) < 0) {
		perror("FS_IOC_FIEMAP");
		free(fm);
		close(fd);
		return 1;
	}

	uint32_t count = fm->fm_mapped_extents;
	if (count == 0) {
		fprintf(stderr, "no extents found\n");
		free(fm);
		close(fd);
		return 1;
	}

	if (count > 0 &&
	    !(fm->fm_extents[count - 1].fe_flags & FIEMAP_EXTENT_LAST)) {
		fprintf(stderr,
		        "extent list truncated at %u entries (MAX_EXTENTS=%d); "
		        "raise MAX_EXTENTS and rebuild\n", count, MAX_EXTENTS);
		free(fm);
		close(fd);
		return 1;
	}

	struct extent_cache_record *records = calloc(count, sizeof(*records));
	if (!records) {
		perror("calloc");
		free(fm);
		close(fd);
		return 1;
	}

	for (uint32_t i = 0; i < count; i++) {
		struct fiemap_extent *fe = &fm->fm_extents[i];
		records[i].file_offset = fe->fe_logical;
		records[i].slba = fe->fe_physical / lba_size;
		records[i].length = fe->fe_length;

		/* Cap the last extent by the file's logical size: FIEMAP
		 * returns on-disk extent length, which may include trailing
		 * block padding past EOF. */
		uint64_t end = records[i].file_offset + records[i].length;
		if (end > file_size)
			records[i].length = file_size - records[i].file_offset;
	}

	free(fm);
	close(fd);

	FILE *out = fopen(argv[2], "wb");
	if (!out) {
		perror("fopen output");
		free(records);
		return 1;
	}

	struct extent_cache_header hdr = {
	        .extent_count = count,
	};
	strncpy(hdr.bdf, bdf, sizeof(hdr.bdf) - 1);

	if (fwrite(&hdr, sizeof(hdr), 1, out) != 1 ||
	    fwrite(records, sizeof(*records), count, out) != count) {
		perror("fwrite");
		fclose(out);
		free(records);
		return 1;
	}
	fclose(out);

	fprintf(stderr, "wrote %u extents to %s (bdf=%s)\n", count, argv[2],
	        bdf);

	free(records);
	return 0;
}
