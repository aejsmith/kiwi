/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               File removal command.
 */

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum rm_mode {
    RM_NORMAL,
    RM_FORCE,
    RM_INTERACTIVE,
} rm_mode_t;

static rm_mode_t rm_mode = RM_NORMAL;
static bool rm_recursive = false;

static void usage(void) {
    printf("Usage: rm [-fiRr] file...\n");
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

static const char *type_string(const char *path) {
    struct stat st;

    if (lstat(path, &st) != 0)
        return "";

    switch (st.st_mode & S_IFMT) {
        case S_IFREG:   return "file";
        case S_IFDIR:   return "directory";
        case S_IFLNK:   return "symbolic link";
        case S_IFBLK:   return "block device";
        case S_IFSOCK:  return "socket";
        case S_IFCHR:   return "character device";
        case S_IFIFO:   return "FIFO";
        default:        return "";
    }
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
        fprintf(stderr, "rm: opendir(%s): %s\n", path, strerror(errno));
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
                perror("rm: malloc");
                free_entries(entries);
                return false;
            }
        }

        entries->entries[entries->count] = strdup(dent->d_name);
        if (!entries->entries[entries->count]) {
            perror("rm: malloc");
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
        if (rm_mode != RM_FORCE)
            fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));

        return false;
    }

    bool writeable = S_ISLNK(st.st_mode) || access(path, W_OK) == 0;

    if (S_ISDIR(st.st_mode)) {
        if (!rm_recursive) {
            fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(EISDIR));
            return false;
        } else if ((rm_mode == RM_INTERACTIVE || (!writeable && isatty(STDIN_FILENO))) && rm_mode != RM_FORCE) {
            fprintf(stderr, "rm: descend into %sdirectory '%s'? ", (!writeable) ? "write-protected " : "", path);
            if (!get_response())
                return true;
        }

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

        if (rm_mode == RM_INTERACTIVE) {
            fprintf(stderr, "rm: remove directory '%s'? ", path);
            if (!get_response())
                return true;
        }

        if (rmdir(path) != 0) {
            fprintf(stderr, "rm: cannot remove directory '%s': %s\n", path, strerror(errno));
            return false;
        }
    } else {
        if ((rm_mode == RM_INTERACTIVE || (!writeable && isatty(STDIN_FILENO))) && rm_mode != RM_FORCE) {
            fprintf(stderr, "rm: remove %s%s '%s'? ", (!writeable) ? "write-protected " : "", type_string(path), path);
            if (!get_response())
                return true;
        }

        if (unlink(path) != 0) {
            fprintf(stderr, "rm: cannot remove '%s': %s\n", path, strerror(errno));
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    int c;
    while ((c = getopt(argc, argv, "fiRr")) != -1) {
        switch (c) {
            case 'f':
                rm_mode = RM_FORCE;
                break;
            case 'i':
                rm_mode = RM_INTERACTIVE;
                break;
            case 'R':
            case 'r':
                rm_recursive = true;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need at least one file argument. */
    if (optind >= argc) {
        usage();
        return EXIT_FAILURE;
    }

    /* Go through everything specified. */
    int ret = EXIT_SUCCESS;

    for (int i = optind; i < argc; i++) {
        if (!do_remove(argv[i]))
            ret = EXIT_FAILURE;
    }

    return ret;
}
