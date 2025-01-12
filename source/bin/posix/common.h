/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX utilities common functions.
 */

#pragma once

#include <core/log.h>

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Get a yes/no response from stdin. */
static inline bool get_response(void) {
    char buf[128];

    if (!fgets(buf, sizeof(buf), stdin)) {
        return false;
    } else if (buf[0] != 'y' && buf[0] != 'Y') {
        return false;
    }

    return true;
}

/** Duplicate a file. */
static inline bool duplicate_file(const char *source, const char *dest) {
    // TODO: When copying, should duplicate all properties such as times as per
    // POSIX spec.

    int source_fd = open(source, O_RDONLY);
    if (source_fd < 0) {
        core_log(CORE_LOG_ERROR, "open(%s): %s", source, strerror(errno));
        return false;
    }

    struct stat st;
    if (fstat(source_fd, &st) != 0) {
        core_log(CORE_LOG_ERROR, "fstat(%s): %s", source, strerror(errno));
        return false;
    }

    int dest_fd = open(dest, O_WRONLY | O_CREAT | O_EXCL, st.st_mode);
    if (dest_fd < 0) {
        core_log(CORE_LOG_ERROR, "open(%s): %s", dest, strerror(errno));
        close(source_fd);
        return false;
    }

    char *data = malloc(st.st_blksize);
    if (data == NULL) {
        perror("mv: malloc");
        close(source_fd);
        close(dest_fd);
        return false;
    }

    /* Copy the file data. */
    off_t copied = 0;
    while (copied < st.st_size) {
        ssize_t ret = read(source_fd, data, st.st_blksize);
        if (ret < 0) {
            core_log(CORE_LOG_ERROR, "read(%s): %s", source, strerror(errno));
            break;
        } else if (ret == 0) {
            break;
        }

        if (write(dest_fd, data, ret) <= 0) {
            core_log(CORE_LOG_ERROR, "write(%s): %s", dest, strerror(errno));
            break;
        }

        copied += ret;
    }

    close(source_fd);
    close(dest_fd);
    free(data);
    return copied == st.st_size;
}

typedef struct dir_entries {
    char **entries;
    size_t count;
} dir_entries_t;

#define DIR_ENTRIES_ALLOC_SIZE 16

/** Free directory entries returned by read_entries(). */
static inline void free_entries(dir_entries_t *entries) {
    for (size_t i = 0; i < entries->count; i++)
        free(entries->entries[i]);

    free(entries->entries);
}

/** Read an array of directory entries. Excludes '.' and '..'. */
static inline bool read_entries(const char *path, dir_entries_t *entries) {
    entries->entries = NULL;
    entries->count   = 0;

    DIR *dir = opendir(path);
    if (!dir) {
        core_log(CORE_LOG_ERROR, "opendir(%s): %s", path, strerror(errno));
        return false;
    }

    size_t alloc_size = 0;

    while (true) {
        struct dirent *dent = readdir(dir);
        if (!dent)
            break;

        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;

        if (entries->count == alloc_size) {
            alloc_size += DIR_ENTRIES_ALLOC_SIZE;

            entries->entries = realloc(entries->entries, alloc_size * sizeof(char *));
            if (!entries->entries) {
                core_log(CORE_LOG_ERROR, "malloc: %s", strerror(errno));
                free_entries(entries);
                return false;
            }
        }

        entries->entries[entries->count] = strdup(dent->d_name);
        if (!entries->entries[entries->count]) {
            core_log(CORE_LOG_ERROR, "malloc: %s", strerror(errno));
            free_entries(entries);
            return false;
        }

        entries->count++;
    }

    closedir(dir);
    return true;
}
