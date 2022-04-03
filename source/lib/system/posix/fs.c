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
 * @brief               POSIX filesystem functions.
 *
 * TODO:
 *  - Implement security (ownership, modes).
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include "stdlib/environ.h"

#include "libsystem.h"

/**
 * umask inheritance across execve() is implemented by storing it in the
 * environment.
 */
#define UMASK_ENV_NAME  "__SYS_POSIX_UMASK"
#define DEFAULT_UMASK   022

/** Current file mode creation mask. */
mode_t current_umask = DEFAULT_UMASK;

/** umask initialisation. */
static __sys_init_prio(LIBSYSTEM_INIT_PRIO_POSIX_UMASK) void posix_umask_init(void) {
    char *str = getenv(UMASK_ENV_NAME);
    if (str) {
        unsigned long val = strtoul(str, NULL, 8);
        if (val != ULONG_MAX)
            current_umask = val & 0777;

        unsetenv(UMASK_ENV_NAME);
    }
}

/** Save umask to the environment before an execve(). */
void posix_fs_exec(environ_t *env) {
    if (current_umask != DEFAULT_UMASK) {
        char str[5];
        snprintf(str, sizeof(str), "%o", current_umask);
        environ_set(env, UMASK_ENV_NAME, str, 1);
    }
}

/** Checks whether access to a file is allowed.
 * @param path          Path to file to check.
 * @param mode          Mode to check (F_OK, or any of the flags R_OK, W_OK and
 *                      X_OK).
 * @return              0 if access is allowed, -1 if not with errno set
 *                      accordingly. */
int access(const char *path, int mode) {
    status_t ret;

    file_info_t info;
    ret = kern_fs_info(path, true, &info);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    if (mode != F_OK) {
        uint32_t access = 0;
        if (mode & R_OK)
            access |= FILE_ACCESS_READ;
        if (mode & W_OK)
            access |= FILE_ACCESS_WRITE;
        if (mode & X_OK)
            access |= FILE_ACCESS_EXECUTE;

        handle_t handle;
        ret = kern_fs_open(path, access, 0, 0, &handle);
        if (ret != STATUS_SUCCESS) {
            libsystem_status_to_errno(ret);
            return -1;
        }

        kern_handle_close(handle);
    }

    return 0;
}

/** Sets the current working directory.
 * @param path          Path to change to.
 * @return              0 on success, -1 on failure with errno set accordingly. */
int chdir(const char *path) {
    if (!path || !path[0]) {
        errno = ENOENT;
        return -1;
    }

    status_t ret = kern_fs_set_curr_dir(path);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return ret;
}

/** Gets the path to the current working directory.
 * @param buf           Buffer to place path string in.
 * @param size          Size of the buffer. If it is too small, errno will be
 *                      set to ERANGE.
 * @return              Pointer to buffer or NULL on failure. */
char *getcwd(char *buf, size_t size) {
    if (!size || !buf) {
        errno = EINVAL;
        return NULL;
    }

    status_t ret = kern_fs_curr_dir(buf, size);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return NULL;
    }

    return buf;
}

/** Gets the path to the current working directory.
 * @param buf           Buffer to place path string in. Must be at least
 *                      PATH_MAX bytes long.
 * @return              Pointer to buffer or NULL on failure. */
char *getwd(char *buf) {
    if (!getcwd(buf, PATH_MAX)) {
        if (errno == ERANGE)
            errno = ENAMETOOLONG;

        return NULL;
    }

    return buf;
}

/** Changes a file's mode.
 * @param path          Path to file.
 * @param mode          New mode for the file.
 * @return              0 on success, -1 on failure. */
int chmod(const char *path, mode_t mode) {
    libsystem_stub("chmod", false);
    return -1;
}

/** Changes a file's mode.
 * @param fd            File descriptor to file.
 * @param mode          New mode for the file.
 * @return              0 on success, -1 on failure. */
int fchmod(int fd, mode_t mode) {
    libsystem_stub("fchmod", false);
    return -1;
}

/** Changes the owner of a filesystem entry.
 * @param path          Path to entry.
 * @param uid           New user ID.
 * @param gid           New group ID.
 * @return              0 on success, -1 on failure. */
int chown(const char *path, uid_t uid, gid_t gid) {
    libsystem_stub("chown", false);
    return -1;
}

/** Changes the owner of a filesystem entry.
 * @param path          Path to entry. If this refers to a symbolic link, it
 *                      will not be dereferenced.
 * @param uid           New user ID.
 * @param gid           New group ID.
 * @return              0 on success, -1 on failure. */
int lchown(const char *path, uid_t uid, gid_t gid) {
    libsystem_stub("lchown", false);
    return -1;
}

/** Changes the owner of a filesystem entry.
 * @param fd            File descriptor to entry.
 * @param uid           New user ID.
 * @param gid           New group ID.
 * @return              0 on success, -1 on failure. */
int fchown(int fd, uid_t uid, gid_t gid) {
    libsystem_stub("fchown", false);
    return -1;
}

/** Creates a hard link.
 * @param source        Source path.
 * @param dest          Destination path.
 * @return              0 on success, -1 on failure. */
int link(const char *source, const char *dest) {
    status_t ret = kern_fs_link(dest, source);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Creates a directory.
 * @param path          Path to directory.
 * @param mode          Mode to create directory with.
 * @return              0 on success, -1 on failure. */
int mkdir(const char *path, mode_t mode) {
    // TODO: Convert mode to ACL.
    status_t ret = kern_fs_create_dir(path);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Creates a filesystem node.
 * @param path          Path to node to create.
 * @param mode          Mode to give the node.
 * @param dev           Device number.
 * @return              0 on success, -1 on failure. */
int mknod(const char *path, mode_t mode, dev_t dev) {
    libsystem_stub("mknod", false);
    return -1;
}

/** Reads the destination of a symbolic link.
 * @param path          Path to symbolc link.
 * @param buf           Buffer to read into.
 * @param size          Size of buffer.
 * @return              Number of bytes written to the buffer on success, or -1
 *                      on failure. */
ssize_t readlink(const char *path, char *buf, size_t size) {
    status_t ret;

    /* The kernel will not do anything if the buffer provided is too small, but
     * we must return the truncated string if it is too small. Find out the link
     * size, and allocate a large enough buffer if the given one is too small. */
    file_info_t info;
    ret = kern_fs_info(path, false, &info);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    char *tmp = NULL;
    if (info.size >= (offset_t)size) {
        tmp = malloc(info.size + 1);
        if (!tmp)
            return -1;
    }

    ret = kern_fs_read_symlink(path, (tmp) ? tmp : buf, info.size + 1);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    if (tmp) {
        memcpy(buf, tmp, size);
        free(buf);
        return size;
    } else {
        return info.size;
    }
}

/** Removes a directory from the filesystem.
 * @param path          Path to directory to remove.
 * @return              0 on success, -1 on failure. */
int rmdir(const char *path) {
    /* Must fail if the last part of the path is . or .. */
    const char *tmp = strrchr(path, '/');
    if (!tmp)
        tmp = path;

    if (tmp[0] == '.' && (tmp[1] == 0 || (tmp[1] == '.' && tmp[2] == 0))) {
        errno = EINVAL;
        return -1;
    }

    /* Our unlink() implementation allows directory removal. However, rmdir() is
     * supposed to return an error if not used on a directory. Therefore, we
     * must use lstat() to determine whether or not the path is a directory
     * first. */
    struct stat st;
    if (lstat(path, &st) != 0) {
        return -1;
    } else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    return unlink(path);
}

/** Convert a kernel file_info_t to a stat structure. */
static void file_info_to_stat(file_info_t *info, struct stat *restrict st) {
    memset(st, 0, sizeof(*st));

    st->st_dev     = info->mount;
    st->st_ino     = info->id;
    st->st_nlink   = info->links;
    st->st_size    = info->size;
    st->st_blksize = info->block_size;
    st->st_atime   = (info->accessed / 1000000000);
    st->st_mtime   = (info->modified / 1000000000);
    st->st_ctime   = (info->created / 1000000000);
    st->st_uid     = 0;//security->uid;
    st->st_gid     = 0;//security->gid;

    /* TODO. */
    st->st_blocks = 0;

    /* Determine the file type mode. */
    switch (info->type) {
        case FILE_TYPE_REGULAR:
            st->st_mode = S_IFREG;
            break;
        case FILE_TYPE_DIR:
            st->st_mode = S_IFDIR;
            break;
        case FILE_TYPE_SYMLINK:
            st->st_mode = S_IFLNK;
            break;
        case FILE_TYPE_BLOCK:
            st->st_mode = S_IFBLK;
            break;
        case FILE_TYPE_CHAR:
            st->st_mode = S_IFCHR;
            break;
        case FILE_TYPE_PIPE:
            st->st_mode = S_IFIFO;
            break;
        case FILE_TYPE_SOCKET:
            st->st_mode = S_IFSOCK;
            break;
    }

    // TODO: Convert mode.
    st->st_mode |= 0755;
}

/** Gets information about a filesystem entry.
 * @param fd            File descriptor to entry.
 * @param st            Structure to fill in.
 * @return              0 on success, -1 on failure. */
int fstat(int fd, struct stat *st) {
    file_info_t info;
    status_t ret = kern_file_info(fd, &info);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    file_info_to_stat(&info, st);
    return 0;
}

/** Gets information about a filesystem entry.
 * @param path          Path to entry. If it refers to a symbolic link, it will
 *                      not be followed.
 * @param st            Structure to fill in.
 * @param               0 on success, -1 on failure. */
int lstat(const char *restrict path, struct stat *restrict st) {
    file_info_t info;
    status_t ret = kern_fs_info(path, false, &info);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    file_info_to_stat(&info, st);
    return 0;
}

/** Gets information about a filesystem entry.
 * @param path          Path to entry. If it refers to a symbolic link, it will
 *                      be followed.
 * @param st            Structure to fill in.
 * @param               0 on success, -1 on failure. */
int stat(const char *restrict path, struct stat *restrict st) {
    file_info_t info;
    status_t ret = kern_fs_info(path, true, &info);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    file_info_to_stat(&info, st);
    return 0;
}

/** Creates a symbolic link.
 * @param dest          Destination of the link.
 * @param path          Path name for the link.
 * @return              0 on success, -1 on failure. */
int symlink(const char *dest, const char *path) {
    status_t ret = kern_fs_create_symlink(path, dest);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Flushes changes to a file to disk.
 * @param fd            Descriptor for file to flush. */
int fsync(int fd) {
    status_t ret;

    unsigned type;
    ret = kern_object_type(fd, &type);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    switch (type) {
        case OBJECT_TYPE_FILE:
            ret = kern_file_sync(fd);
            if (ret != STATUS_SUCCESS) {
                libsystem_status_to_errno(ret);
                return -1;
            }

            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

/** Flushes filesystem caches. */
void sync(void) {
    kern_fs_sync();
}

/** Sets the file mode creation mask.
 * @param mask          New mask.
 * @return              Previous mask. */
mode_t umask(mode_t mask) {
    mode_t prev = current_umask;
    current_umask = mask & 0777;
    return prev;
}

/**
 * Removes an entry from a directory in the filesystem. If no more links remain
 * to the file the entry refers to, it will be removed.
 *
 * @param path          Path to unlink.
 *
 * @return              0 on success, -1 on failure.
 */
int unlink(const char *path) {
    status_t ret = kern_fs_unlink(path);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}

/** Sets file access and modification times.
 * @param path          Path to file.
 * @param times         Structure containing new times.
 * @return              0 on success, -1 on failure. */
int utime(const char *path, const struct utimbuf *times) {
    libsystem_stub("utime", false);
    return -1;
}
