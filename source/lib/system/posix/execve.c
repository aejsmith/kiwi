/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		POSIX program execution function.
 */

#include <kernel/process.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "posix_priv.h"

/** Execute a file with an interpreter.
 * @param fd		File descriptor referring to file.
 * @param path		Path to file.
 * @param argv		Arguments for process (NULL-terminated array).
 * @param envp		Environment for process (NULL-terminated array).
 * @return		Does not return on success, -1 on failure. */
static int do_interp(int fd, const char *path, char *const argv[], char *const envp[]) {
	errno = ENOSYS;
	close(fd);
	return -1;
}

/**
 * Execute a binary.
 *
 * Executes a binary with the given arguments and a copy of the provided
 * environment block.
 *
 * @param path		Path to binary to execute.
 * @param argv		Arguments for process (NULL-terminated array).
 * @param envp		Environment for process (NULL-terminated array).
 *
 * @return		Does not return on success, -1 on failure.
 */
int execve(const char *path, char *const argv[], char *const envp[]) {
	status_t ret;
	char buf[2];
	int fd;

	if(access(path, X_OK) != 0) {
		errno = EACCES;
		return -1;
	}

	/* Open the file and check if it is an interpreter. */
	fd = open(path, O_RDONLY);
	if(fd < 0)
		return -1;

	if(read(fd, buf, 2) == 2 && strncmp(buf, "#!", 2) == 0)
		return do_interp(fd, path, argv, envp);

	close(fd);

	ret = kern_process_exec(path, (const char *const *)argv,
		(const char *const *)envp, 0, NULL, -1);
	libsystem_status_to_errno(ret);
	return -1;
}
