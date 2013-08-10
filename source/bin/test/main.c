/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Test application.
 */

#include <kernel/mutex.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int i, status;
	int32_t *addr;
	status_t ret;
	pid_t pid;

	printf("Hello, World! My arguments are:\n");
	for(i = 0; i < argc; i++)
		printf(" argv[%d] = '%s'\n", i, argv[i]);

	ret = kern_vm_map((void **)&addr, 0x1000, VM_ADDRESS_ANY,
		VM_PROT_READ | VM_PROT_WRITE, 0, INVALID_HANDLE,
		0, NULL);
	if(ret != STATUS_SUCCESS) {
		printf("Failed to create mapping: %d\n", ret);
		return EXIT_FAILURE;
	}

	pid = fork();
	if(pid == 0) {
		kern_mutex_lock(addr, -1);
		printf("Child process is process %d, sleeping\n", getpid());
		kern_mutex_unlock(addr);

		sleep(5);
		addr[1] = 0xDEADBEEF;

		kern_mutex_lock(addr, -1);
		printf("Child process finishing\n");
		kern_mutex_unlock(addr);

		return 123;
	} else if(pid > 0) {
		kern_mutex_lock(addr, -1);
		printf("Parent process %d got child %d, waiting\n", getpid(), pid);
		kern_mutex_unlock(addr);

		pid = wait(&status);
		if(pid < 0) {
			perror("wait");
			return EXIT_FAILURE;
		}

		printf("Process %d finished (status: 0x%x/%d)\n", pid, status, WEXITSTATUS(status));
		return EXIT_SUCCESS;
	} else {
		perror("fork");
		return EXIT_FAILURE;
	}
}
