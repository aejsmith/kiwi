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

int main(int argc, char **argv) {
	int i;
	int32_t *addr;
	handle_t handle;
	status_t ret;

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

	ret = kern_process_clone(&handle);

	kern_mutex_lock(addr, -1);
	printf("Returned %d (handle: %d) in process %d\n", ret, handle,
		kern_process_id(PROCESS_SELF));
	kern_mutex_unlock(addr);

	while(true) { kern_thread_sleep(1000000, NULL); }
}
