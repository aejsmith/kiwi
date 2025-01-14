/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX memory management functions.
 */

#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/vm.h>

#include <sys/mman.h>

#include <errno.h>

#include "libsystem.h"

/**
 * Creates a new virtual memory mapping in the calling process' address space.
 * If MAP_ANONYMOUS is specified in flags, the mapping will be backed with
 * anonymous pages. Otherwise, it will be backed by the file referred to by the
 * file descriptor given.
 *
 * @param start         If MAP_FIXED is specified, this is the exact address to
 *                      map at. Otherwise, it is taken as a hint to the
 *                      position of the mapping, and a region of free space will
 *                      be allocated.
 * @param size          Size of the mapping (multiple of page size).
 * @param prot          Flags describing the required access to the mapping.
 * @param flags         Mapping behaviour flags.
 * @param fd            File descriptor to back mapping with.
 * @param offset        Offset into the file to map from (multiple of page size).
 *
 * @return              Address of mapping on success, MAP_FAILED on failure
 *                      (with errno set appropriately).
 */
void *mmap(void *start, size_t size, int prot, int flags, int fd, off_t offset) {
    status_t ret;

    if (!size) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    /* Through the POSIX interface, only allow files to be mapped. */
    unsigned type;
    ret = kern_object_type(fd, &type);
    if (ret != STATUS_SUCCESS || type != OBJECT_TYPE_FILE) {
        errno = EBADF;
        return MAP_FAILED;
    }

    unsigned kspec = (flags & MAP_FIXED) ? VM_ADDRESS_EXACT : VM_ADDRESS_ANY;

    uint32_t kaccess = 0;
    if (prot & PROT_READ)
        kaccess |= VM_ACCESS_READ;
    if (prot & PROT_WRITE)
        kaccess |= VM_ACCESS_WRITE;
    if (prot & PROT_EXEC)
        kaccess |= VM_ACCESS_EXECUTE;

    uint32_t kflags = 0;
    if ((flags & (MAP_PRIVATE | MAP_SHARED)) == MAP_PRIVATE) {
        kflags |= VM_MAP_PRIVATE;
    } else if ((flags & (MAP_PRIVATE | MAP_SHARED)) != MAP_SHARED) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    ret = kern_vm_map(&start, size, 0, kspec, kaccess, kflags, fd, offset, NULL);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return MAP_FAILED;
    }

    return start;
}

/** Unmaps a region of virtual memory.
 * @param start         Start of the mapping.
 * @param size          Size of the mapping.
 * @return              0 on success, -1 on failure (with errno set appropriately). */
int munmap(void *start, size_t size) {
    status_t ret = kern_vm_unmap(start, size);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

int mprotect(void *addr, size_t len, int prot) {
    libsystem_stub(__func__, false);
    return -1;
}

int mlock(const void *addr, size_t len) {
    libsystem_stub(__func__, false);
    return -1;
}

int munlock(const void *addr, size_t len) {
    libsystem_stub(__func__, false);
    return -1;
}
