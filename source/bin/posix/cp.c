/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief		        File copy command.
 */

#include <core/path.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>

#include "common.h"

typedef enum cp_mode {
    CP_NORMAL,
    CP_FORCE,
    CP_INTERACTIVE,
} cp_mode_t;

static cp_mode_t cp_mode = CP_NORMAL;
static bool cp_recursive = false;

static void usage(void) {
    printf("Usage: cp [-fi] file target_file\n");
    printf("       cp [-fiR] file... target\n");
}

static bool do_copy(const char *source, const char *dest) {
    int ret;

    struct stat source_st;
    ret = lstat(source, &source_st);
    if (ret != 0) {
        core_log(CORE_LOG_ERROR, "lstat(%s): %s", source, strerror(errno));
        return false;
    }

    struct stat dest_st;
    ret = lstat(dest, &dest_st);
    if (ret != 0 && errno != ENOENT) {
        core_log(CORE_LOG_ERROR, "lstat(%s): %s", dest, strerror(errno));
        return false;
    }

    bool dest_exists = ret == 0;
    if (S_ISDIR(source_st.st_mode)) {
        if (!cp_recursive) {
            core_log(CORE_LOG_ERROR, "cannot copy '%s': %s", source, strerror(EISDIR));
            return false;
        } else if (dest_exists) {
            if (!S_ISDIR(dest_st.st_mode)) {
               core_log(CORE_LOG_ERROR, "cannot overwrite non-directory '%s' with directory '%s'", dest, source);
                return false;
            }
        } else {
            if (mkdir(dest, source_st.st_mode) != 0) {
                core_log(CORE_LOG_ERROR, "mkdir(%s): %s", dest, strerror(errno));
                return false;
            }
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

            if (!do_copy(source_buf, dest_buf)) {
                free_entries(&entries);
                return false;
            }
        }

        free_entries(&entries);
    } else {
        if (dest_exists) {
            if (S_ISDIR(dest_st.st_mode)) {
                core_log(CORE_LOG_ERROR, "cannot overwrite directory '%s' with non-directory '%s'", dest, source);
                return false;
            } else {
                bool writeable = S_ISLNK(dest_st.st_mode) || access(dest, W_OK) == 0;

                if ((cp_mode == CP_INTERACTIVE || (!writeable && isatty(STDIN_FILENO))) && cp_mode != CP_FORCE) {
                    core_log(CORE_LOG_ERROR, "overwrite %sfile '%s'? ", (!writeable) ? "write-protected " : "", dest);
                    if (!get_response())
                        return true;
                }

                if (unlink(dest) != 0) {
                    core_log(CORE_LOG_ERROR, "unlink(%s): %s", dest, strerror(errno));
                    return false;
                }
            }
        }

        if (S_ISLNK(source_st.st_mode) && cp_recursive) {
            char link_buf[PATH_MAX];
            ret = readlink(source, link_buf, PATH_MAX);
            if (ret < 0) {
                core_log(CORE_LOG_ERROR, "readlink(%s): %s", source, strerror(errno));
                return false;
            }

            link_buf[ret] = 0;

            if (symlink(link_buf, dest) != 0) {
                core_log(CORE_LOG_ERROR, "symlink(%s, %s): %s", link_buf, dest, strerror(errno));
                return false;
            }
        } else {
            if (!duplicate_file(source, dest))
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
        switch(c) {
            case 'f':
                cp_mode = CP_FORCE;
                break;
            case 'i':
                cp_mode = CP_INTERACTIVE;
                break;
            case 'R':
            case 'r':
                cp_recursive = true;
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
        core_log(CORE_LOG_ERROR, "stat: %s", strerror(errno));
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
        /* Work out the destination path. */
        if (is_dir) {
            char *name = core_path_basename(argv[i]);
            if (!name) {
                errno = ENOMEM;
                core_log(CORE_LOG_ERROR, "malloc: %s", strerror(errno));
                continue;
            }

            char *dest = malloc(strlen(name) + strlen(argv[argc - 1]) + 2);
            if (!dest) {
                free(name);
                core_log(CORE_LOG_ERROR, "malloc: %s", strerror(errno));
                continue;
            }

            sprintf(dest, "%s/%s", argv[argc - 1], name);

            if (!do_copy(argv[i], dest))
                ret = EXIT_FAILURE;

            free(dest);
            free(name);
        } else {
            if (!do_copy(argv[i], argv[argc - 1]))
                ret = EXIT_FAILURE;
        }
    }

    return ret;
}
