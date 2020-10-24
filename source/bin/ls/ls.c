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
 * @brief               Directory list command.
 */

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

/** Structure containing a list of directory entries. */
typedef struct dir_entries {
    struct dirent **dents;
    struct stat *stat;
    char **fullpath;
    int count;
} dir_entries_t;

/** Macro to get a string that resets output colour. */
#define RESET_COLOUR    ((use_colour) ? "\e[0m" : "")

/** Macro to get a colour string for a dangling symlink. */
#define LINK_COLOUR     ((use_colour) ? "\e[1;31;40m" : "")

/** Whether the output device is a terminal. */
static bool is_terminal = false;

/** Whether to use colour. */
static bool use_colour = false;

/** Whether to output with the long format. */
static bool long_format = false;

/** Whether to recursively list subdirectories. */
static bool recursive = false;

/** Whether to show all files, including those starting with . */
static bool show_all = false;

/** Whether to given sizes in human-readable form. */
static bool human_readable = false;

static void usage() {
    printf("Usage: ls [-CRahl] file...\n");
}

static int terminal_width(void) {
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col != 0) {
        return size.ws_col;
    } else {
        return 80;
    }
}

static const char *process_mode(struct stat *st, char mode[11]) {
    if (mode) {
        switch (st->st_mode & S_IFMT) {
            case S_IFREG:   mode[0] = '-'; break;
            case S_IFDIR:   mode[0] = 'd'; break;
            case S_IFLNK:   mode[0] = 'l'; break;
            case S_IFBLK:   mode[0] = 'b'; break;
            case S_IFSOCK:  mode[0] = 's'; break;
            case S_IFCHR:   mode[0] = 'c'; break;
            case S_IFIFO:   mode[0] = 'f'; break;
            default:        mode[0] = '?'; break;
        }

        mode[1] = (st->st_mode & S_IRUSR) ? 'r' : '-';
        mode[2] = (st->st_mode & S_IWUSR) ? 'w' : '-';
        mode[3] = (st->st_mode & S_IXUSR) ? 'x' : '-';
        mode[4] = (st->st_mode & S_IRGRP) ? 'r' : '-';
        mode[5] = (st->st_mode & S_IWGRP) ? 'w' : '-';
        mode[6] = (st->st_mode & S_IXGRP) ? 'x' : '-';
        mode[7] = (st->st_mode & S_IROTH) ? 'r' : '-';
        mode[8] = (st->st_mode & S_IWOTH) ? 'w' : '-';
        mode[9] = (st->st_mode & S_IXOTH) ? 'x' : '-';

        mode[10] = 0;
    }

    if (use_colour) {
        switch (st->st_mode & S_IFMT) {
            case S_IFDIR:               return "\e[1;34m";
            case S_IFLNK:               return "\e[1;36m";
            case S_IFBLK: case S_IFCHR: return "\e[1;33m";
            case S_IFSOCK:              return "\e[1;35m";
            case S_IFIFO:               return "\e[33m";
        }

        if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            return "\e[1;32m";
    }

    return "";
}

static void convert_size(off_t size, char *buf, size_t count) {
    if (size < 1024) {
        snprintf(buf, count, "%llu", size);
    } else if (size < (1024 * 1024)) {
        snprintf(buf, count, "%lluK", (size / 1024));
    } else if (size < (1024 * 1024 * 1024)) {
        snprintf(buf, count, "%lluM", (size / 1024 / 1024));
    } else {
        snprintf(buf, count, "%lluG", (size / 1024 / 1024 / 1024));
    }
}

static int scandir_filter(const struct dirent *dent) {
    return (show_all || dent->d_name[0] != '.') ? 1 : 0;
}

static int scandir_compare(const void *a, const void *b) {
    const struct dirent *d1 = *(const struct dirent **)a;
    const struct dirent *d2 = *(const struct dirent **)b;
    return strcasecmp(d1->d_name, d2->d_name);
}

/** Frees the array from scandir(). */
static void scandir_free(struct dirent **dents, int count) {
    int i;

    for (i = 0; i < count; i++) {
        free(dents[i]);
    }

    free(dents);
}

static void do_list_long(dir_entries_t *dents, const char *dir) {
    for (int i = 0; i < dents->count; i++) {
        char mode[11];
        const char *colour = process_mode(&dents->stat[i], mode);

        struct tm *tm = localtime(&dents->stat[i].st_mtime);
        if (!tm) {
            perror("ls: localtime");
            continue;
        }

        char date[20];
        strftime(date, sizeof(date), "%F %H:%M", tm);

        char link[PATH_MAX];
        bool is_link = S_ISLNK(dents->stat[i].st_mode);
        if (is_link) {
            ssize_t size = readlink(dents->fullpath[i], link, sizeof(link));
            if (size < 0) {
                perror("ls: readlink");
                continue;
            }

            link[size] = 0;
        }

        printf("%s %2u ", mode, dents->stat[i].st_nlink);

        if (human_readable) {
            char size_str[13];
            convert_size(dents->stat[i].st_size, size_str, sizeof(size_str));
            printf("%12s ", size_str);
        } else {
            printf("%12llu ", dents->stat[i].st_size);
        }

        printf("%s %s%s%s", date, colour, dents->dents[i]->d_name, RESET_COLOUR);

        if (is_link) {
            char dest[PATH_MAX];
            if (link[0] == '/') {
                snprintf(dest, PATH_MAX, "%s", link);
            } else {
                snprintf(dest, PATH_MAX, "%s/%s", dir, link);
            }

            struct stat st;
            if (lstat(dest, &st) == 0) {
                printf(" -> %s%s%s\n", process_mode(&st, NULL), link, RESET_COLOUR);
            } else {
                printf(" -> %s%s%s\n", LINK_COLOUR, link, RESET_COLOUR);
            }
        } else {
            printf("\n");
        }
    }
}

static void do_list_short(dir_entries_t *dents) {
    if (is_terminal) {
        int max = 0;
        for (int i = 0; i < dents->count; i++) {
            int len = strlen(dents->dents[i]->d_name);
            max = ((len + 2) > max) ? (len + 2) : max;
        }

        /* Work out what we can fit on one row. */
        int num = (terminal_width() - 1) / max;
        if (num < 1)
            num = 1;

        int count = 0;
        for (int i = 0; i < dents->count; i++) {
            printf(
                "%s%-*s%s",
                process_mode(&dents->stat[i], NULL), max,
                dents->dents[i]->d_name, RESET_COLOUR);

            if (++count == num) {
                count = 0;
                printf("\n");
            }
        }

        if (count != 0)
            printf("\n");
    } else {
        for (int i = 0; i < dents->count; i++)
            printf("%s\n", dents->dents[i]->d_name);
    }
}

static bool do_list(const char *path, bool print_name) {
    static bool done_first = false;

    bool success = true;

    dir_entries_t dents = {};
    dents.count = scandir(path, &dents.dents, scandir_filter, scandir_compare);

    bool single = false;
    if (dents.count < 0) {
        if (errno != ENOTDIR) {
            perror("ls: scandir");
            success = false;
            goto out;
        }

        /* Handle a single entry here. Create a fake dirent for it. */
        dents.dents = malloc(sizeof(dents.dents[0]));
        if (!dents.dents) {
            perror("ls: malloc");
            success = false;
            goto out;
        }

        dents.dents[0] = malloc(sizeof(struct dirent) + strlen(path) + 1);
        if (!dents.dents[0]) {
            perror("ls: malloc");
            success = false;
            goto out;
        }

        strcpy(dents.dents[0]->d_name, path);
        dents.count = 1;
        single = true;
    } else {
        if (print_name) {
            if (done_first) {
                printf("\n%s:\n", path);
            } else {
                printf("%s:\n", path);
                done_first = true;
            }
        }

        if (dents.count == 0) {
            scandir_free(dents.dents, dents.count);
            goto out;
        }
    }

    /* Allocate arrays to store information on each entry. Use of calloc() is
     * important here as cleanup relies on unallocated entries being NULL. */
    dents.fullpath = calloc(dents.count, sizeof(char *));
    if (!dents.fullpath) {
        perror("ls: calloc");
        success = false;
        goto out;
    }

    dents.stat = calloc(dents.count, sizeof(struct stat));
    if (!dents.stat) {
        perror("ls: calloc");
        success = false;
        goto out;
    }

    /* For each entry, get a full path name and stat information for it. */
    for (int i = 0; i < dents.count; i++) {
        if (single) {
            dents.fullpath[i] = malloc(strlen(path) + 1);
        } else {
            dents.fullpath[i] = malloc(strlen(path) + strlen(dents.dents[i]->d_name) + 2);
        }

        if (!dents.fullpath[i]) {
            perror("ls: malloc");
            success = false;
            goto out;
        }

        if (single) {
            strcpy(dents.fullpath[i], path);
        } else {
            sprintf(dents.fullpath[i], "%s/%s", path, dents.dents[i]->d_name);
        }

        if (lstat(dents.fullpath[i], &dents.stat[i]) != 0) {
            perror("ls: lstat");
            success = false;
            goto out;
        }
    }

    /* Print out the information according to the required format. */
    if (long_format) {
        do_list_long(&dents, path);
    } else {
        do_list_short(&dents);
    }

    /* Recurse if required. */
    if (recursive) {
        for (int i = 0; i < dents.count; i++) {
            if (strcmp(dents.dents[i]->d_name, ".") == 0 || strcmp(dents.dents[i]->d_name, "..") == 0)
                continue;

            if (S_ISDIR(dents.stat[i].st_mode))
                do_list(dents.fullpath[i], true);
        }
    }

out:
    if (dents.dents)
        scandir_free(dents.dents, dents.count);

    if (dents.fullpath) {
        for (int i = 0; i < dents.count; i++) {
            if (dents.fullpath[i])
                free(dents.fullpath[i]);
        }

        free(dents.fullpath);
    }

    if (dents.stat)
        free(dents.stat);

    return success;
}

/** Main function of the ls command. */
int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    /* Parse options. */
    int c;
    while ((c = getopt(argc, argv, "CRahl")) != -1) {
        switch (c) {
            case 'C':
                long_format = false;
                break;
            case 'R':
                recursive = true;
                break;
            case 'a':
                show_all = true;
                break;
            case 'h':
                human_readable = true;
                break;
            case 'l':
                long_format = true;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Check if we're outputting to a terminal. */
    if (isatty(STDOUT_FILENO)) {
        is_terminal = true;

        // TODO: Colour - detect compatible terminal type.
    }

    int ret = EXIT_SUCCESS;

    /* Loop through each specified entry. */
    if (optind >= argc) {
        /* If we're in recursive mode we should print out directory names. */
        if (!do_list(".", recursive))
            ret = EXIT_FAILURE;
    } else {
        for (int i = optind; i < argc; i++) {
            if (!do_list(argv[i], (recursive || (argc - optind) > 1) ? true : false))
                ret = EXIT_FAILURE;
        }
    }

    return ret;
}
