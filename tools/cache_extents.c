/*
 * cache_extents - Walk a directory tree, FIEMAP each regular file,
 * and write the resulting extent map to disk in the multi-file
 * extent_cache format consumed by fs_mock.
 *
 * Usage: cache_extents <root_dir> <output.bin>
 *
 * All files under <root_dir> must reside on the same XFS filesystem
 * sitting on a single NVMe namespace (or partition). The PCI BDF and
 * LBA size are resolved once from the first file encountered; later
 * files are assumed to share them.
 */

#define _GNU_SOURCE

#include "extent_cache.h"

#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
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
#define NFTW_MAX_FDS 32

struct file_entry {
	char *path;
	struct extent_cache_extent *extents;
	uint32_t n_extents;
};

static struct {
	struct file_entry *files;
	uint32_t n_files;
	uint32_t cap;
	char bdf[16];
	uint32_t lba_size;
	uint64_t part_start;
	int resolved;
	int abort_rc;
} g;

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

/* FIEMAP reports fe_physical relative to the containing block device.
 * If the file lives on a partition the caller sees partition-relative
 * offsets, but xNVMe issues reads against namespace-relative LBAs. Add
 * the partition's start (always in 512-byte sectors) so slba is
 * namespace-relative. Returns 0 for whole-device filesystems. */
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
fiemap_file(int fd, uint64_t file_size,
            struct extent_cache_extent **out_extents, uint32_t *out_count)
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

	struct extent_cache_extent *ext = calloc(count, sizeof(*ext));
	if (!ext) {
		free(fm);
		return -ENOMEM;
	}

	for (uint32_t i = 0; i < count; i++) {
		struct fiemap_extent *fe = &fm->fm_extents[i];
		ext[i].file_offset = fe->fe_logical;
		ext[i].slba = (fe->fe_physical + g.part_start) / g.lba_size;
		ext[i].length = fe->fe_length;

		/* Cap the last extent by the file's logical size: FIEMAP
		 * returns on-disk extent length, which may include trailing
		 * block padding past EOF. */
		uint64_t end = ext[i].file_offset + ext[i].length;
		if (end > file_size)
			ext[i].length = file_size - ext[i].file_offset;
	}

	free(fm);
	*out_extents = ext;
	*out_count = count;
	return 0;
}

static int
ensure_resolved(int fd)
{
	if (g.resolved)
		return 0;

	if (resolve_pci_bdf(fd, g.bdf, sizeof(g.bdf)) < 0) {
		fprintf(stderr, "error: could not resolve PCI BDF\n");
		return -ENOENT;
	}
	g.lba_size = get_lba_size(fd);
	g.part_start = get_partition_start_bytes(fd);
	g.resolved = 1;
	return 0;
}

static int
visit_file(const char *path, uint64_t file_size)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", path, strerror(errno));
		return -errno;
	}

	int rc = ensure_resolved(fd);
	if (rc < 0) {
		close(fd);
		return rc;
	}

	struct extent_cache_extent *extents = NULL;
	uint32_t count = 0;
	rc = fiemap_file(fd, file_size, &extents, &count);
	close(fd);
	if (rc < 0)
		return rc;
	if (count == 0) {
		free(extents);
		return 0;
	}

	if (g.n_files == g.cap) {
		uint32_t new_cap = g.cap ? g.cap * 2 : 64;
		struct file_entry *grown = realloc(
		        g.files, new_cap * sizeof(*grown));
		if (!grown) {
			free(extents);
			return -ENOMEM;
		}
		g.files = grown;
		g.cap = new_cap;
	}

	struct file_entry *fe = &g.files[g.n_files++];
	fe->path = strdup(path);
	fe->extents = extents;
	fe->n_extents = count;
	if (!fe->path) {
		free(extents);
		return -ENOMEM;
	}
	return 0;
}

static int
nftw_cb(const char *fpath, const struct stat *sb, int typeflag,
        struct FTW *ftwbuf)
{
	(void)ftwbuf;
	if (typeflag != FTW_F || !S_ISREG(sb->st_mode))
		return 0;
	int rc = visit_file(fpath, (uint64_t)sb->st_size);
	if (rc < 0) {
		g.abort_rc = rc;
		return -1;
	}
	return 0;
}

static int
write_cache(const char *out_path)
{
	FILE *out = fopen(out_path, "wb");
	if (!out) {
		perror("fopen output");
		return -errno;
	}

	struct extent_cache_header hdr = {
	        .magic = EXTENT_CACHE_MAGIC,
	        .version = EXTENT_CACHE_VERSION,
	        .n_files = g.n_files,
	};
	memcpy(hdr.bdf, g.bdf, sizeof(hdr.bdf));

	if (fwrite(&hdr, sizeof(hdr), 1, out) != 1)
		goto err;

	for (uint32_t i = 0; i < g.n_files; i++) {
		struct file_entry *fe = &g.files[i];
		uint32_t path_len = (uint32_t)strlen(fe->path) + 1;
		struct extent_cache_file_record fr = {
		        .path_len = path_len,
		        .n_extents = fe->n_extents,
		};
		if (fwrite(&fr, sizeof(fr), 1, out) != 1)
			goto err;
		if (fwrite(fe->path, 1, path_len, out) != path_len)
			goto err;
		if (fwrite(fe->extents, sizeof(*fe->extents),
		           fe->n_extents, out) != fe->n_extents)
			goto err;
	}

	fclose(out);
	return 0;

err:
	perror("fwrite");
	fclose(out);
	return -EIO;
}

int
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <root_dir> <output.bin>\n", argv[0]);
		return 1;
	}

	if (nftw(argv[1], nftw_cb, NFTW_MAX_FDS, FTW_PHYS) != 0) {
		if (g.abort_rc < 0)
			fprintf(stderr, "walk aborted: %s\n",
			        strerror(-g.abort_rc));
		else
			perror("nftw");
		return 1;
	}

	if (g.n_files == 0) {
		fprintf(stderr, "no regular files under %s\n", argv[1]);
		return 1;
	}
	if (!g.resolved) {
		fprintf(stderr, "could not resolve NVMe device for %s\n",
		        argv[1]);
		return 1;
	}

	if (write_cache(argv[2]) < 0)
		return 1;

	fprintf(stderr,
	        "wrote %u files to %s (bdf=%s lba=%u part_start=%lu)\n",
	        g.n_files, argv[2], g.bdf, g.lba_size, g.part_start);

	for (uint32_t i = 0; i < g.n_files; i++) {
		free(g.files[i].path);
		free(g.files[i].extents);
	}
	free(g.files);
	return 0;
}
