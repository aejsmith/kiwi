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
 * @brief               POSIX file descriptor functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "posix/posix.h"

/** Converts POSIX open() flags to kernel flags.
 * @param oflag         POSIX open flags.
 * @param _kaccess      Where to store kernel access rights.
 * @param _kflags       Where to store kernel flags.
 * @param _kcreate      Where to store kernel creation flags. */
static void convert_open_flags(int oflag, uint32_t *_kaccess, uint32_t *_kflags, unsigned *_kcreate) {
    uint32_t kaccess = 0;
    uint32_t kflags = 0;

    if (oflag & O_RDONLY)
        kaccess |= FILE_ACCESS_READ;
    if (oflag & O_WRONLY)
        kaccess |= FILE_ACCESS_WRITE;

    *_kaccess = kaccess;

    if (oflag & O_NONBLOCK)
        kflags |= FILE_NONBLOCK;
    if (oflag & O_APPEND)
        kflags |= FILE_APPEND;

    *_kflags = kflags;

    if (oflag & O_CREAT) {
        *_kcreate = (oflag & O_EXCL) ? FS_MUST_CREATE : FS_CREATE;
    } else {
        *_kcreate = 0;
    }
}

/** Opens a file or directory.
 * @param path          Path to file to open.
 * @param oflag         Flags controlling how to open the file.
 * @param ...           Mode to create the file with if O_CREAT is specified.
 * @return              File descriptor referring to file (positive value) on
 *                      success, -1 on failure (errno will be set to the error
 *                      reason). */
int open(const char *path, int oflag, ...) {
    status_t ret;

    /* Check whether the arguments are valid. TODO: I'm not sure if the second
     * check is correct, POSIX doesn't say anything about O_CREAT with
     * O_DIRECTORY. */
    if (!(oflag & O_RDWR) || (oflag & O_EXCL && !(oflag & O_CREAT))) {
        errno = EINVAL;
        return -1;
    } else if (oflag & O_CREAT && oflag & O_DIRECTORY) {
        errno = EINVAL;
        return -1;
    } else if (!(oflag & O_WRONLY) && oflag & O_TRUNC) {
        errno = EACCES;
        return -1;
    }

    /* If O_CREAT is specified, we assume that we're going to be opening a file.
     * Although POSIX doesn't specify anything about O_CREAT with a directory,
     * Linux fails with EISDIR if O_CREAT is used with a directory that already
     * exists. */
    file_type_t type;
    if (oflag & O_CREAT) {
        type = FILE_TYPE_REGULAR;
    } else {
        file_info_t info;
        ret = kern_fs_info(path, true, &info);
        if (ret != STATUS_SUCCESS) {
            libsystem_status_to_errno(ret);
            return -1;
        }

        type = info.type;

        if (oflag & O_DIRECTORY && type != FILE_TYPE_DIR) {
            errno = ENOTDIR;
            return -1;
        }
    }

    /* Convert the flags to kernel flags. */
    uint32_t kaccess, kflags;
    unsigned kcreate;
    convert_open_flags(oflag, &kaccess, &kflags, &kcreate);

    /* Open according to the entry type. */
    handle_t handle;
    switch (type) {
        case FILE_TYPE_DIR:
            if (oflag & O_WRONLY || oflag & O_TRUNC) {
                errno = EISDIR;
                return -1;
            }

            /* Fallthrough. */
        case FILE_TYPE_REGULAR:
            #if 0
            if (oflag & O_CREAT) {
                /* Obtain the creation mask. */
                va_start(args, oflag);
                mode = va_arg(args, mode_t);
                va_end(args);

                /* Apply the creation mode mask. */
                mode &= ~current_umask;

                /* Convert the mode to a kernel ACL. */
                security.acl = posix_mode_to_acl(NULL, mode);
                if (!security.acl)
                  return -1;
            }
            #endif

            /* Open the file, creating it if necessary. */
            ret = kern_fs_open(path, kaccess, kflags, kcreate, &handle);
            if (ret != STATUS_SUCCESS) {
                libsystem_status_to_errno(ret);
                return -1;
            }

            /* Truncate the file if requested. */
            if (oflag & O_TRUNC) {
                ret = kern_file_resize(handle, 0);
                if (ret != STATUS_SUCCESS && ret != STATUS_NOT_REGULAR) {
                    kern_handle_close(handle);
                    libsystem_status_to_errno(ret);
                    return -1;
                }
            }

            break;

        default:
            errno = ENOTSUP;
            return -1;
    }

    /* Mark the handle as inheritable if not opening with O_CLOEXEC. */
    if (!(oflag & O_CLOEXEC))
        kern_handle_set_flags(handle, HANDLE_INHERITABLE);

    return (int)handle;
}

/**
 * Opens a file, creating it if it does not exist. If it does exist, it will be
 * truncated to zero length.
 *
 * @param path          Path to file.
 * @param mode          Mode to create file with if it doesn't exist.
 *
 * @return              File descriptor referring to file (positive value) on
 *                      success, -1 on failure (errno will be set to the error
 *                      reason).
 */
int creat(const char *path, mode_t mode) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/** Closes a file descriptor.
 * @param fd            File descriptor to close.
 * @return              0 on success, -1 on failure with errno set appropriately. */
int close(int fd) {
    status_t ret = kern_handle_close(fd);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

static int fcntl_dupfd(int fd, int dest, bool cloexec) {
    status_t ret;

    /* TODO: Implement this. */
    if (dest > 0) {
        errno = ENOSYS;
        return -1;
    }

    handle_t new;
    ret = kern_handle_duplicate(fd, INVALID_HANDLE, &new);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    if (!cloexec) {
        ret = kern_handle_set_flags(new, HANDLE_INHERITABLE);
        if (ret != STATUS_SUCCESS) {
            kern_handle_close(new);
            libsystem_status_to_errno(ret);
            return -1;
        }
    }

    return new;
}

static int fcntl_getfd(int fd) {
    uint32_t kflags;
    status_t ret = kern_handle_flags(fd, &kflags);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    int flags = 0;
    flags |= ((kflags & HANDLE_INHERITABLE) ? 0 : FD_CLOEXEC);

    return flags;
}

static int fcntl_setfd(int fd, int flags) {
    uint32_t kflags = 0;
    kflags |= ((flags & FD_CLOEXEC) ? 0 : HANDLE_INHERITABLE);

    status_t ret = kern_handle_set_flags(fd, kflags);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

static int fcntl_getfl(int fd) {
    uint32_t kaccess, kflags;
    status_t ret = kern_file_state(fd, &kaccess, &kflags, NULL);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    int flags = 0;
    flags |= ((kaccess & FILE_ACCESS_READ) ? O_RDONLY : 0);
    flags |= ((kaccess & FILE_ACCESS_WRITE) ? O_WRONLY : 0);
    flags |= ((kflags & FILE_NONBLOCK) ? O_NONBLOCK : 0);
    flags |= ((kflags & FILE_APPEND) ? O_APPEND : 0);

    return flags;
}

static int fcntl_setfl(int fd, int flags) {
    uint32_t kflags = 0;
    kflags |= ((flags & O_NONBLOCK) ? FILE_NONBLOCK : 0);
    kflags |= ((flags & O_APPEND) ? FILE_APPEND : 0);

    status_t ret = kern_file_set_flags(fd, kflags);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/**
 * Controls the behaviour of a file descriptor according to the specified
 * command. The following commands are currently recognised:
 *
 *  - F_DUPFD: Duplicates the given file descriptor. The new descriptor will be
 *    the lowest available that is greater than or equal to the third argument.
 *    It will refer to the same open file description as the old descriptor.
 *    The return value (on success) is the new file descriptor. The new file
 *    descriptor will not have the FD_CLOEXEC flag set, meaning it will remain
 *    open after an exec*() call.
 *  - F_DUPFD_CLOEXEC: The same as F_DUPFD, but instead sets the FD_CLOEXEC
 *    flag on the new descriptor.
 *  - F_GETFD: Get file descriptor flags. These flags are associated with a
 *    single file descriptor, and do not affect other descriptors referring to
 *    the same open file. The return value (on success) is the set of flags
 *    currently set on the FD.
 *  - F_SETFD: Set file descriptor flags (see F_GETFD). The return value (on
 *    success) is 0.
 *  - F_GETFL: Get file status flags and access flags. These flags are stored
 *    for each open file description, and modifying them affects other file
 *    descriptors referring to the same description (FDs duplicated by
 *    dup()/dup2()/F_DUPFD and duplicated by fork() refer to the same file
 *    description). The return value (on success) is the set of flags currently
 *    set on the file description.
 *  - F_SETFL: Set file status flags and access flags (see F_GETFL). The return
 *    value (on success) is 0.
 * 
 * @param fd            File descriptor to control.
 * @param cmd           Command to perform.
 * @param ...           Optional argument specific to the command.
 *
 * @return              Dependent on the command performed on success, -1 on
 *                      failure (errno will be set appropriately).
 */
int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);

    int arg;
    int ret = -1;
    switch (cmd) {
        case F_DUPFD:
            arg = va_arg(args, int);
            ret = fcntl_dupfd(fd, arg, false);
            break;
        case F_DUPFD_CLOEXEC:
            arg = va_arg(args, int);
            ret = fcntl_dupfd(fd, arg, true);
            break;
        case F_GETFD:
            ret = fcntl_getfd(fd);
            break;
        case F_SETFD:
            arg = va_arg(args, int);
            ret = fcntl_setfd(fd, arg);
            break;
        case F_GETFL:
            ret = fcntl_getfl(fd);
            break;
        case F_SETFL:
            arg = va_arg(args, int);
            ret = fcntl_setfl(fd, arg);
            break;
        default:
            errno = EINVAL;
            break;
    }

    va_end(args);
    return ret;
}

/** Duplicates a file descriptor.
 * @param fd            File descriptor to duplicate.
 * @return              New FD, or -1 on failure. */
int dup(int fd) {
    return fcntl(fd, F_DUPFD, 0);
}

/** Duplicates a file descriptor.
 * @param fd            File descriptor to duplicate.
 * @param newfd         New file descriptor (if a file descriptor exists with
 *                      this number, it will be closed).
 * @return              New FD, or -1 on failure. */
int dup2(int fd, int newfd) {
    status_t ret;

    if (newfd < 0) {
        errno = EBADF;
        return -1;
    }

    handle_t new;
    ret = kern_handle_duplicate(fd, newfd, &new);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    ret = kern_handle_set_flags(new, HANDLE_INHERITABLE);
    if (ret != STATUS_SUCCESS) {
        kern_handle_close(new);
        libsystem_status_to_errno(ret);
        return -1;
    }

    return new;
}

/**
 * Changes the offset of a file descriptor according to the specified action.
 * If the action is SEEK_SET, the offset will be set to the exact value given.
 * If it is SEEK_CUR, the offset will be set to the current offset plus the
 * value given. If it is SEEK_END, the offset will be set to the end of the
 * file plus the specified number of bytes.
 *
 * @param fd            File descriptor to change offset of.
 * @param off           Offset value (used according to action).
 * @param act           Action to perform.
 *
 * @return              New file offset, or -1 on failure.
 */
off_t lseek(int fd, off_t off, int act) {
    int kact;
    switch (act) {
        case SEEK_SET:
            kact = FILE_SEEK_SET;
            break;
        case SEEK_CUR:
            kact = FILE_SEEK_ADD;
            break;
        case SEEK_END:
            kact = FILE_SEEK_END;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    offset_t new;
    status_t ret = kern_file_seek(fd, kact, off, &new);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return new;
}
