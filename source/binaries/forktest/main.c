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
 * @brief		Fork test app.
 */

#include <kernel/thread.h>

#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int status;
	pid_t pid;

	pid = fork();
	if(pid == 0) {
		printf("Child 1! Waiting 1 second...\n");
		thread_usleep(1000000);
		return 42;
	} else if(pid < 0) {
		perror("fork");
		return 1;
	}

	pid = fork();
	if(pid == 0) {
		thread_usleep(100000);
		printf("Child 2! Waiting 2 seconds...\n");
		thread_usleep(2000000);
		return 123;
	} else if(pid < 0) {
		perror("fork");
		return 1;
	}

	while(1) {
		pid = waitpid(-1, &status, 0);
		if(pid < 0) {
			if(errno == ECHILD) {
				return 0;
			}
			perror("waitpid");
			return 1;
		}

		if(WIFEXITED(status)) {
			printf("Child %d exited, status=%d\n", pid, WEXITSTATUS(status));
		}
	}
}
