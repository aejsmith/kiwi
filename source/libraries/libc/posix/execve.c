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
 * @brief		POSIX program execution function.
 */

#include <kernel/process.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "posix_priv.h"

/** Execute a file with an interpreter.
 * @param fd		File descriptor referring to file.
 * @param path		Path to file.
 * @param argv		Arguments for process (NULL-terminated array).
 * @param envp		Environment for process (NULL-terminated array).
 * @return		Does not return on success, -1 on failure. */
static int execve_interp(int fd, const char *path, char *const argv[], char *const envp[]) {
	errno = ENOSYS;
	close(fd);
	return -1;
}

/** Execute a binary.
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

	/* Open the file and check if it is an interpreter. FIXME: Execute
	 * permission check. */
	fd = open(path, O_RDONLY);
	if(fd < 0) {
		return -1;
	}
	if(read(fd, buf, 2) == 2 && strncmp(buf, "#!", 2) == 0) {
		return execve_interp(fd, path, argv, envp);
	}
	close(fd);

	ret = kern_process_replace(path, (const char *const *)argv, (const char *const *)envp,
	                           NULL, NULL, -1);
	libc_status_to_errno(ret);
	return -1;
}
