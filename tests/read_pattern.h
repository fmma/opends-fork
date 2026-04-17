/*
 * read_pattern.h - Shared pattern rule for sync-read tests.
 *
 * Both the prep binary (which writes the pattern to a file) and the
 * backend test binaries (which verify reads against the pattern in
 * memory) derive bytes from pattern_byte(). Keeping one definition
 * guarantees prep and verify never drift apart.
 *
 * POSIX only; no OpenDS dependency.
 */
#ifndef READ_PATTERN_H_
#define READ_PATTERN_H_

#include <stddef.h>
#include <sys/types.h>

#define PAGE 4096
#define FILE_PAGES 16
#define FILE_SIZE ((size_t)(PAGE * FILE_PAGES))

static inline unsigned char
pattern_byte(off_t abs_offset)
{
	unsigned long p = (unsigned long)abs_offset / PAGE;
	unsigned long o = (unsigned long)abs_offset % PAGE;
	return (unsigned char)((o + p) & 0xff);
}

#endif /* READ_PATTERN_H_ */
