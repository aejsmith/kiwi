/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               File move command.
 * 
 * TODO:
 *  - When copying, should duplicate all properties such as times as per POSIX
 *    spec.
 */

#include <core/path.h>

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum move_mode {
    MOVE_NORMAL,
    MOVE_FORCE,
    MOVE_INTERACTIVE,
} move_mode_t;

typedef enum move_status {
    MOVE_STATUS_NOTHING,
    MOVE_STATUS_MOVED,
    MOVE_STATUS_COPIED,
    MOVE_STATUS_FAILURE,
} move_status_t;

static move_mode_t move_mode = MOVE_NORMAL;

static void usage(void) {
    printf("Usage: mv [-fi] file target_file\n");
    printf("       mv [-fi] file... target_dir\n");
}

static bool get_response(void) {
    char buf[128];

    if (!fgets(buf, sizeof(buf), stdin)) {
        return false;
    } else if (buf[0] != 'y' && buf[0] != 'Y') {
        return false;
    }

    return true;
}

typedef struct dir_entries {
    char **entries;
    size_t count;
} dir_entries_t;

#define DIR_ENTRIES_ALLOC_SIZE 16

static void free_entries(dir_entries_t *entries) {
    for (size_t i = 0; i < entries->count; i++)
        free(entries->entries[i]);

    free(entries->entries);
}

static bool read_entries(const char *path, dir_entries_t *entries) {
    entries->entries = NULL;
    entries->count   = 0;

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "mv: opendir(%s): %s\n", path, strerror(errno));
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
                perror("mv: malloc");
                free_entries(entries);
                return false;
            }
        }

        entries->entries[entries->count] = strdup(dent->d_name);
        if (!entries->entries[entries->count]) {
            perror("mv: malloc");
            free_entries(entries);
            return false;
        }

        entries->count++;
    }

    closedir(dir);
    return true;
}

static bool do_remove(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "mv: cannot remove '%s': %s\n", path, strerror(errno));
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        /* Read all the entries up front as reading them one at a time while
         * we're removing things in it will cause us to miss entries. */
        dir_entries_t entries;
        if (!read_entries(path, &entries))
            return false;

        for (size_t i = 0; i < entries.count; i++) {
            char buf[PATH_MAX];
            snprintf(buf, PATH_MAX, "%s/%s", path, entries.entries[i]);
            buf[PATH_MAX - 1] = 0;

            if (!do_remove(buf)) {
                free_entries(&entries);
                return false;
            }
        }

        free_entries(&entries);

        if (rmdir(path) != 0) {
            fprintf(stderr, "mv: cannot remove directory '%s': %s\n", path, strerror(errno));
            return false;
        }
    } else {
        if (unlink(path) != 0) {
            fprintf(stderr, "mv: cannot remove '%s': %s\n", path, strerror(errno));
            return false;
        }
    }

    return true;
}

static bool duplicate_file(const char *source, const char *dest) {
    int source_fd = open(source, O_RDONLY);
    if (source_fd < 0) {
        fprintf(stderr, "mv: open(%s): %s\n", source, strerror(errno));
        return false;
    }

    struct stat st;
    if (fstat(source_fd, &st) != 0) {
        fprintf(stderr, "mv: fstat(%s): %s\n", source, strerror(errno));
        return false;
    }

    int dest_fd = open(dest, O_WRONLY | O_CREAT | O_EXCL, st.st_mode);
    if (dest_fd < 0) {
        fprintf(stderr, "mv: open(%s): %s\n", dest, strerror(errno));
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
            fprintf(stderr, "mv: read(%s): %s\n", source, strerror(errno));
            break;
        } else if (ret == 0) {
            break;
        }

        if (write(dest_fd, data, ret) <= 0) {
            fprintf(stderr, "mv: write(%s): %s\n", dest, strerror(errno));
            break;
        }

        copied += ret;
    }

    close(source_fd);
    close(dest_fd);
    free(data);
    return copied == st.st_size;
}

static move_status_t do_move(const char *source, const char *dest) {
    int ret;

    struct stat source_st;
    ret = lstat(source, &source_st);
    if (ret != 0) {
        fprintf(stderr, "mv: lstat(%s): %s\n", source, strerror(errno));
        return MOVE_STATUS_FAILURE;
    }

    struct stat dest_st;
    ret = lstat(dest, &dest_st);
    if (ret != 0 && errno != ENOENT) {
        fprintf(stderr, "mv: lstat(%s): %s\n", dest, strerror(errno));
        return MOVE_STATUS_FAILURE;
    }

    bool dest_exists = ret == 0;
    if (dest_exists) {
        if (S_ISDIR(dest_st.st_mode) && !S_ISDIR(source_st.st_mode)) {
            fprintf(stderr, "mv: cannot overwrite directory '%s' with non-directory '%s'\n", dest, source);
            return MOVE_STATUS_FAILURE;
        } else if (!S_ISDIR(dest_st.st_mode) && S_ISDIR(source_st.st_mode)) {
            fprintf(stderr, "mv: cannot overwrite non-directory '%s' with directory '%s'\n", dest, source);
            return MOVE_STATUS_FAILURE;
        }

        bool writeable = S_ISLNK(dest_st.st_mode) || access(dest, W_OK) == 0;

        if ((move_mode == MOVE_INTERACTIVE || (!writeable && isatty(STDIN_FILENO))) && move_mode != MOVE_FORCE) {
            fprintf(stderr, "mv: overwrite %sfile '%s'? ", (!writeable) ? "write-protected " : "", dest);
            if (!get_response())
                return MOVE_STATUS_NOTHING;
        }
    }

    /* If the source and destination are on the same filesystem our job is easy. */
    ret = rename(source, dest);
    if (ret != 0 && errno != EXDEV) {
        fprintf(stderr, "mv: rename(%s, %s): %s\n", source, dest, strerror(errno));
        return MOVE_STATUS_FAILURE;
    } else if (ret == 0) {
        return MOVE_STATUS_MOVED;
    }

    /* Remove the destination. */
    if (dest_exists) {
        ret = (S_ISDIR(dest_st.st_mode)) ? rmdir(dest) : unlink(dest);
        if (ret != 0) {
            fprintf(stderr, "nv: cannot remove existing destination '%s': %s\n", dest, strerror(errno));
            return MOVE_STATUS_FAILURE;
        }
    }

    if (S_ISDIR(source_st.st_mode)) {
        if (mkdir(dest, source_st.st_mode) != 0) {
            fprintf(stderr, "mv: mkdir(%s): %s\n", dest, strerror(errno));
            return MOVE_STATUS_FAILURE;
        }

        dir_entries_t entries;
        if (!read_entries(source, &entries))
            return false;

        for (size_t i = 0; i < entries.count; i++) {
            char source_buf[PATH_MAX];
            snprintf(source_buf, PATH_MAX, "%s/%s", source, entries.entries[i]);
            source_buf[PATH_MAX - 1] = 0;

            char dest_buf[PATH_MAX];
            snprintf(dest_buf, PATH_MAX, "%s/%s", dest, entries.entries[i]);
            dest_buf[PATH_MAX - 1] = 0;

            if (do_move(source_buf, dest_buf) != MOVE_STATUS_COPIED) {
                free_entries(&entries);
                return MOVE_STATUS_FAILURE;
            }
        }

        free_entries(&entries);
    } else if (S_ISLNK(source_st.st_mode)) {
        char link_buf[PATH_MAX];
        ret = readlink(source, link_buf, PATH_MAX);
        if (ret < 0) {
            fprintf(stderr, "mv: readlink(%s): %s\n", source, strerror(errno));
            return MOVE_STATUS_FAILURE;
        }

        link_buf[ret] = 0;

        if (symlink(link_buf, dest) != 0) {
            fprintf(stderr, "mv: symlink(%s, %s): %s\n", link_buf, dest, strerror(errno));
            return MOVE_STATUS_FAILURE;
        }
    } else {
        if (!duplicate_file(source, dest))
            return MOVE_STATUS_FAILURE;
    }

    return MOVE_STATUS_COPIED;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    int c;
    while ((c = getopt(argc, argv, "fi")) != -1) {
        switch (c) {
            case 'f':
                move_mode = MOVE_FORCE;
                break;
            case 'i':
                move_mode = MOVE_INTERACTIVE;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need at least two file arguments. */
    if ((argc - optind) < 2) {
        usage();
        return EXIT_FAILURE;
    }

    /* Find the destination and check whether it is a directory. */
    bool is_dir = false;
    struct stat st;
    if (stat(argv[argc - 1], &st) != 0 && errno != ENOENT) {
        perror("stat");
        return EXIT_FAILURE;
    } else if (errno != ENOENT && S_ISDIR(st.st_mode)) {
        is_dir = true;
    }

    if (!is_dir && (argc - optind) != 2) {
        usage();
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;

    for (int i = optind; i < (argc - 1); i++) {
        move_status_t status;

        /* Work out the destination path. */
        if (is_dir) {
            char *name = core_path_basename(argv[i]);
            if (!name) {
                errno = ENOMEM;
                perror("malloc");
                continue;
            }

            char *dest = malloc(strlen(name) + strlen(argv[argc - 1]) + 2);
            if (!dest) {
                free(name);
                perror("malloc");
                continue;
            }

            sprintf(dest, "%s/%s", argv[argc - 1], name);

            status = do_move(argv[i], dest);

            free(dest);
            free(name);
        } else {
            status = do_move(argv[i], argv[argc - 1]);
        }

        if (status == MOVE_STATUS_FAILURE) {
            ret = EXIT_FAILURE;
        } else if (status == MOVE_STATUS_COPIED) {
            if (!do_remove(argv[i]))
                ret = EXIT_FAILURE;
        }
    }

    return ret;
}
