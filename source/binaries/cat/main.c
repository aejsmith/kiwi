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
 * @brief		File concatenation command.
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Print out the contents of a file.
 * @param file		File to output.
 * @return		Whether successful. */
static bool cat_file(const char *file) {
	bool success = true;
	char *buf = NULL;
	struct stat st;
	ssize_t ret;
	int fd;

	if(strcmp(file, "-") == 0) {
		fd = 0;
	} else {
		fd = open(file, O_RDONLY);
		if(fd < 0) {
			perror("cat: open");
			return false;
		}
	}

	if(fstat(fd, &st) != 0) {
		perror("cat: fstat");
		close(fd);
		return false;
	}

	if(st.st_blksize == 0) {
		fprintf(stderr, "cat: warning: st_blksize is 0\n");
		st.st_blksize = 1;
	}

	buf = malloc(st.st_blksize);
	if(!buf) {
		perror("cat: malloc");
		close(fd);
		return false;
	}

	while(true) {
		ret = read(fd, buf, st.st_blksize);
		if(ret < 0) {
			perror("cat: read");
			success = false;
			break;
		} else if(ret == 0) {
			break;
		}

		fwrite(buf, ret, 1, stdout);
	}

	free(buf);
	close(fd);
	return success;
} 

/** Main function for the cat command. */
int main(int argc, char **argv) {
	int ret = EXIT_SUCCESS, i;

	if(argc >= 2 && strcmp(argv[1], "--help") == 0) {
		printf("Usage: %s <file...>\n", argv[0]);
		return EXIT_SUCCESS;
	}

	if(argc < 2) {
		if(!cat_file("-")) {
			ret = EXIT_FAILURE;
		}
	} else {
		for(i = 1; i < argc; i++) {
			if(!cat_file(argv[i])) {
				ret = EXIT_FAILURE;
			}
		}
	}

	return ret;
}
