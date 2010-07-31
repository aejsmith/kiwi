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
 * @brief		Kernel library heap functions.
 */

#include <kernel/semaphore.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>

#include "../libkernel.h"

/** Statically allocated heap. */
static uint8_t libkernel_heap[LIBKERNEL_HEAP_SIZE];
static size_t libkernel_heap_current = 0;

/** Semaphore to protect the heap. */
static handle_t libkernel_heap_lock;

/** Allocate some memory.
 * @param size		Size to allocate.
 * @return		Pointer to allocation on success, NULL on failure. */
void *malloc(size_t size) {
	void *ret;

	semaphore_down(libkernel_heap_lock, -1);

	if((libkernel_heap_current + size) > LIBKERNEL_HEAP_SIZE) {
		semaphore_up(libkernel_heap_lock, 1);
		return NULL;
	}

	ret = &libkernel_heap[libkernel_heap_current];
	libkernel_heap_current += size;
	semaphore_up(libkernel_heap_lock, 1);
	return ret;
}

/** Free memory previously allocated with malloc().
 * @param addr		Address allocated. */
void free(void *addr) {
	/* Nothing happens. It probably should sometime. TODO. */
}

/** Initialise the libkernel heap. */
void libkernel_heap_init(void) {
	status_t ret;

	/* Create the semaphore that protects the heap. */
	ret = semaphore_create("libkernel_heap_lock", 1, &libkernel_heap_lock);
	if(ret != STATUS_SUCCESS) {
		printf("libkernel: could not create heap lock (%d)\n", ret);
		process_exit(ret);
	}
}
