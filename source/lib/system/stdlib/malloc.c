/*
 * Copyright (C) 2009-2022 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Memory allocation functions.
 */

#include <core/mutex.h>
#include <core/utility.h>

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/system.h>
#include <kernel/vm.h>

#include <errno.h>
#include <stddef.h>
#include <time.h>

#include "libsystem.h"

#define LACKS_SYS_MMAN_H
#define LACKS_STDLIB_H

#define HAVE_MMAP           1
#define HAVE_MREMAP         0
#define HAVE_MORECORE       0
#define NO_MALLINFO         1

#define ABORT \
    libsystem_fatal("dlmalloc abort");
#define USAGE_ERROR_ACTION(m, p) \
    libsystem_fatal( \
        "dlmalloc usage error (%s:%d): %p, %p (ret: %p)", \
        __FUNCTION__, __LINE__, m, p, __builtin_return_address(0));
#define MALLOC_FAILURE_ACTION \
    errno = ENOMEM;

/** Get the system page size. */
#define malloc_getpagesize \
    __extension__ \
    ({ \
        size_t __size; \
        kern_system_info(SYSTEM_INFO_PAGE_SIZE, &__size); \
        __size; \
    })

/** Wrapper for allocations. */
static inline void *mmap_wrapper(size_t size) {
    status_t ret;
    void *addr;

    ret = kern_vm_map(
        &addr, size, 0, VM_ADDRESS_ANY, VM_ACCESS_READ | VM_ACCESS_WRITE,
        VM_MAP_PRIVATE, INVALID_HANDLE, 0, "dlmalloc");
    if (ret != STATUS_SUCCESS)
        return (void *)-1;

    return addr;
}

/** Wrapper for freeing. */
static inline int munmap_wrapper(void *start, size_t length) {
    return kern_vm_unmap(start, length);
}

#define MMAP(s)             mmap_wrapper((s))
#define DIRECT_MMAP(s)      mmap_wrapper((s))
#define MUNMAP(a, s)        munmap_wrapper((a), (s))

/* To stop it defining dev_zero_fd. */
#define MAP_ANONYMOUS       0

static inline void init_lock(core_mutex_t *lock) {
    *lock = CORE_MUTEX_INITIALIZER;
}

#define USE_LOCKS           2
#define MLOCK_T             core_mutex_t
#define INITIAL_LOCK(sl)    init_lock((sl))
#define ACQUIRE_LOCK(sl)    core_mutex_lock((sl), -1)
#define RELEASE_LOCK(sl)    core_mutex_unlock((sl))

static MLOCK_T malloc_global_mutex = CORE_MUTEX_INITIALIZER;

#pragma clang diagnostic ignored "-Wnull-pointer-arithmetic"

/*
 * Work around clang being "clever". The implementation of calloc() calls
 * malloc(), then checks the chunk header flags to see if it needs to zero.
 * clang thinks that because the memory returned by malloc() hasn't been touched
 * at all, then reads from it are undefined, so it doesn't bother doing so and
 * just removes the check/memset entirely.
 *
 * To avoid this, rename all the functions, and then wrap them with the proper
 * names. This stops clang from applying it's special behaviours for calling
 * malloc().
 */
#define DLMALLOC_EXPORT extern __sys_hidden
#define USE_DL_PREFIX 1

#include "dlmalloc.c"

void *malloc(size_t size) {
    return dlmalloc(size);
}

void *calloc(size_t nmemb, size_t size) {
    return dlcalloc(nmemb, size);
}

void *realloc(void *ptr, size_t size) {
    return dlrealloc(ptr, size);
}

void free(void *ptr) {
    return dlfree(ptr);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    return dlposix_memalign(memptr, alignment, size);
}
