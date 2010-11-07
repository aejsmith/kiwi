/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Directory list command.
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
#define RESET		((is_terminal) ? "\e[0m" : "")

/** Macro to get a colour string for a dangling symlink. */
#define LINK_COLOUR	((is_terminal) ? "\e[1;31;40m" : "")

/** Whether the output device is a terminal. */
static bool is_terminal = false;

/** Whether to output with the long format. */
static bool long_format = false;

/** Whether to recursively list subdirectories. */
static bool recursive = false;

/** Whether to show all files, including those starting with . */
static bool show_all = false;

/** Whether to given sizes in human-readable form. */
static bool human_readable = false;

/** Print a usage message.
 * @param argv0		Program name. */
static void usage(char *argv0) {
	printf("Usage: %s [-CRahl] file...\n", argv0);
}

/** Get the terminal width.
 * @return		Width of the terminal in characters. */
static int terminal_width(void) {
	struct winsize size;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col != 0) {
		return size.ws_col;
	} else {
		return 80;
	}
}

/** Create a mode and colour string for a directory entry.
 * @param st		Stat structure for file.
 * @param mode		Where to store mode string (if NULL only colour will
 *			be returned). Must be 11 bytes long.
 * @return		Colour string for file. */
static const char *process_mode(struct stat *st, char *mode) {
	if(mode) {
		switch(st->st_mode & S_IFMT) {
		case S_IFREG:	mode[0] = '-'; break;
		case S_IFDIR:	mode[0] = 'd'; break;
		case S_IFLNK:	mode[0] = 'l'; break;
		case S_IFBLK:	mode[0] = 'b'; break;
		case S_IFSOCK:	mode[0] = 's'; break;
		case S_IFCHR:	mode[0] = 'c'; break;
		case S_IFIFO:	mode[0] = 'f'; break;
		default:	mode[0] = '?'; break;
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

	if(is_terminal) {
		switch(st->st_mode & S_IFMT) {
		case S_IFDIR:			return "\e[1;34m";
		case S_IFLNK:			return "\e[1;36m";
		case S_IFBLK: case S_IFCHR:	return "\e[1;33m";
		case S_IFSOCK:			return "\e[1;35m";
		case S_IFIFO:			return "\e[33m";
		}

		if(st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
			return "\e[1;32m";
		}
	}

	return "";
}

/** Create a human-readable size string.
 * @param size		Size of the file.
 * @param buf		Buffer to place string into.
 * @param count		Size of the buffer. */
static void convert_size(off_t size, char *buf, size_t count) {
	if(size < 1024) {
		snprintf(buf, count, "%llu", size);
	} else if(size < (1024 * 1024)) {
		snprintf(buf, count, "%lluK", (size / 1024));
	} else if(size < (1024 * 1024 * 1024)) {
		snprintf(buf, count, "%lluM", (size / 1024 / 1024));
	} else {
		snprintf(buf, count, "%lluG", (size / 1024 / 1024 / 1024));
	}
}

/** Filters out directory entries starting with . if not showing everything. */
static int scandir_filter(const struct dirent *dent) {
	return (show_all || dent->d_name[0] != '.') ? 1 : 0;
}

/** Comparision function for scandir(). Same as alphasort() but ignores case. */
static int scandir_compare(const void *a, const void *b) {
	const struct dirent *d1 = *(const struct dirent **)a;
        const struct dirent *d2 = *(const struct dirent **)b;
        return strcasecmp(d1->d_name, d2->d_name);
}

/** Frees the array from scandir(). */
static void scandir_free(struct dirent **dents, int count) {
	int i;

	for(i = 0; i < count; i++) {
		free(dents[i]);
	}

	free(dents);
}

/** Do a list of directory entries in long format.
 * @param argv0		Program name.
 * @param dents		Directory entries to list.
 * @param dir		Path of directory. */
static void do_list_long(const char *argv0, dir_entries_t *dents, const char *dir) {
	char mode[11], date[20], link[PATH_MAX], tmp[PATH_MAX], fsize[13];
	const char *colour;
	ssize_t size = 0;
	struct stat st;
	struct tm *tm;
	int i;

	for(i = 0; i < dents->count; i++) {
		size = 0;
		colour = process_mode(&dents->stat[i], mode);
		tm = localtime(&dents->stat[i].st_mtime);
		if(!tm) {
			fprintf(stderr, "%s: localtime: %s\n", argv0, strerror(errno));
			continue;
		}
		strftime(date, sizeof(date), "%F %H:%M", tm);

		if(S_ISLNK(dents->stat[i].st_mode)) {
			size = readlink(dents->fullpath[i], link, sizeof(link));
			if(size < 0) {
				fprintf(stderr, "%s: readlink: %s\n", argv0, strerror(errno));
				continue;
			}

			link[size] = 0;
		}

		printf("%s %2u ", mode, dents->stat[i].st_nlink);
		if(human_readable) {
			convert_size(dents->stat[i].st_size, fsize, sizeof(fsize));
			printf("%12s ", fsize);
		} else {
			printf("%12llu ", dents->stat[i].st_size);
		}
		printf("%s %s%s%s", date, colour, dents->dents[i]->d_name, RESET);

		if(size > 0) {
			if(link[0] == '/') {
				snprintf(tmp, PATH_MAX, "%s", link);
			} else {
				snprintf(tmp, PATH_MAX, "%s/%s", dir, link);
			}

			if(lstat(tmp, &st) == 0) {
				printf(" -> %s%s%s\n", process_mode(&st, NULL), link, RESET);
			} else {
				printf(" -> %s%s%s\n", LINK_COLOUR, link, RESET);
			}
		} else {
			printf("\n");
		}
	}
}

/** Do a list of directory entries in short format.
 * @param dents		Directory entries to list. */
static void do_list_short(dir_entries_t *dents) {
	int i, num, len, max = 0, count;

	//if(is_terminal) {
		for(i = 0; i < dents->count; i++) {
			len = strlen(dents->dents[i]->d_name);
			max = ((len + 2) > max) ? (len + 2) : max;
		}

		/* Work out what we can fit on one row. */
		num = (terminal_width() - 1) / max;
		if(num < 1) {
			num = 1;
		}

		for(i = 0, count = 0; i < dents->count; i++) {
			printf("%s%-*s%s", process_mode(&dents->stat[i], NULL), max,
			       dents->dents[i]->d_name, RESET);
			if(++count == num) {
				count = 0;
				printf("\n");
			}
		}

		if(count != 0) {
			printf("\n");
		}
	//} else {
	//	for(i = 0; i < dents->count; i++) {
	//		printf("%s\n", dents->dents[i]->d_name);
	//	}
	//}
}

/** Perform the real work of listing a command line argument. */
static int do_list(const char *argv0, const char *path, bool print_name) {
	static bool had_first = false;
	dir_entries_t dents;
	bool single = false;
	int i, ret = 0;

	memset(&dents, 0, sizeof(dents));

	dents.count = scandir(path, &dents.dents, scandir_filter, scandir_compare);
	if(dents.count < 0) {
		if(errno != ENOTDIR) {
			fprintf(stderr, "%s: scandir: %s\n", argv0, strerror(errno));
			ret = 1;
			goto out;
		}

		/* Handle a single entry here. Create a fake dirent for it. */
		dents.dents = malloc(sizeof(dents.dents[0]));
		if(!dents.dents) {
			fprintf(stderr, "%s: malloc: %s\n", argv0, strerror(errno));
			ret = 1;
			goto out;
		}
		dents.dents[0] = malloc(sizeof(struct dirent) + strlen(path) + 1);
		if(!dents.dents[0]) {
			fprintf(stderr, "%s: malloc: %s\n", argv0, strerror(errno));
			ret = 1;
			goto out;
		}

		strcpy(dents.dents[0]->d_name, path);
		dents.count = 1;
		single = true;
	} else {
		if(print_name) {
			if(had_first) {
				printf("\n%s:\n", path);
			} else {
				printf("%s:\n", path);
				had_first = true;
			}
		}

		if(dents.count == 0) {
			scandir_free(dents.dents, dents.count);
			goto out;
		}
	}

	/* Allocate arrays to store information on each entry. Use of
	 * calloc() is important here as failure code relies on unallocated
	 * entries being NULL. */
	dents.fullpath = calloc(dents.count, sizeof(char *));
	if(!dents.fullpath) {
		fprintf(stderr, "%s: malloc: %s\n", argv0, strerror(errno));
		ret = 1;
		goto out;
	}
	dents.stat = calloc(dents.count, sizeof(struct stat));
	if(!dents.stat) {
		fprintf(stderr, "%s: malloc: %s\n", argv0, strerror(errno));
		ret = 1;
		goto out;
	}

	/* For each entry, get a full path name and stat information for it. */
	for(i = 0; i < dents.count; i++) {
		if(single) {
			dents.fullpath[i] = malloc(strlen(path) + 1);
		} else {
			dents.fullpath[i] = malloc(strlen(path) + strlen(dents.dents[i]->d_name) + 2);
		}
		if(!dents.fullpath[i]) {
			fprintf(stderr, "%s: malloc: %s\n", argv0, strerror(errno));
			ret = 1;
			goto out;
		}

		if(single) {
			strcpy(dents.fullpath[i], path);
		} else {
			sprintf(dents.fullpath[i], "%s/%s", path, dents.dents[i]->d_name);
		}

		/* Get information on the file. */
		if(lstat(dents.fullpath[i], &dents.stat[i]) != 0) {
			fprintf(stderr, "%s: lstat: %s\n", argv0, strerror(errno));
			ret = 1;
			goto out;
		}
	}

	/* Print out the information according to the required format. */
	if(long_format) {
		do_list_long(argv0, &dents, path);
	} else {
		do_list_short(&dents);
	}

	/* Recurse if required. */
	if(recursive) {
		for(i = 0; i < dents.count; i++) {
			if(strcmp(dents.dents[i]->d_name, ".") == 0 || strcmp(dents.dents[i]->d_name, "..") == 0) {
				continue;
			}

			if(S_ISDIR(dents.stat[i].st_mode)) {
				do_list(argv0, dents.fullpath[i], true);
			}
		}
	}
out:
	if(dents.dents) {
		scandir_free(dents.dents, dents.count);
	}
	if(dents.fullpath) {
		for(i = 0; i < dents.count; i++) {
			if(dents.fullpath[i]) {
				free(dents.fullpath[i]);
			}
		}
		free(dents.fullpath);
	}
	if(dents.stat) {
		free(dents.stat);
	}

	return ret;
}

/** Main function of the ls command. */
int main(int argc, char *argv[]) {
	int i, c, ret = EXIT_SUCCESS;

	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			return EXIT_SUCCESS;
		}
	}

	/* Parse options. */
	while((c = getopt(argc, argv, "CRahl")) != -1) {
		switch(c) {
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
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* Check if we're outputting to a terminal. */
	if(isatty(STDOUT_FILENO)) {
		// When enabling colours, uncomment bit in short list
		//is_terminal = true;
	}

	/* Loop through each specified entry. */
	if(optind >= argc) {
		/* If we're in recursive mode we should print out directory
		 * names. */
		if(do_list(argv[0], ".", recursive) != 0) {
			ret = EXIT_FAILURE;
		}
	} else {
		for(i = optind; i < argc; i++) {
			if(do_list(argv[0], argv[i], (recursive || (argc - optind) > 1) ? true : false) != 0) {
				ret = EXIT_FAILURE;
			}
		}
	}

	return ret;
}
