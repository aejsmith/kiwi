/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory management functions.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** Protection flags for mmap(). */
#define PROT_NONE       0           /**< No access is given to the region. */
#define PROT_READ       (1<<0)      /**< Region is readable. */
#define PROT_WRITE      (1<<1)      /**< Region is writeable. */
#define PROT_EXEC       (1<<2)      /**< Region is executable. */

/** Mapping flags. */
#define MAP_PRIVATE     (1<<0)      /**< Changes are not shared outside the process. */
#define MAP_SHARED      (1<<1)      /**< Changes are shared. */
#define MAP_FIXED       (1<<2)      /**< Use given start address exactly. */
#define MAP_ANONYMOUS   (1<<3)      /**< Map anonymous memory. */

/** Value returned by mmap() on failure. */
#define MAP_FAILED      ((void *)-1l)

extern int mlock(const void *addr, size_t len);
/* int mlockall(int); */
extern void *mmap(void *start, size_t size, int prot, int flags, int fd, off_t offset);
extern int mprotect(void *addr, size_t len, int prot);
/* int msync(void *, size_t, int); */
extern int munlock(const void *addr, size_t len);
/* int munlockall(void); */
extern int munmap(void *start, size_t size);
/* int posix_madvise(void *, size_t, int); */
/* int shm_open(const char *, int, mode_t); */
/* int shm_unlink(const char *); */

__SYS_EXTERN_C_END
