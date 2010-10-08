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

#include <util/mutex.h>
#include <stdlib.h>
#include "../libkernel.h"

/** Statically allocated heap. */
static uint8_t libkernel_heap[LIBKERNEL_HEAP_SIZE];
static size_t libkernel_heap_current = 0;

/** Lock to protect the heap. */
static LIBC_MUTEX_DECLARE(libkernel_heap_lock);

/** Allocate some memory.
 * @param size		Size to allocate.
 * @return		Pointer to allocation on success, NULL on failure. */
void *malloc(size_t size) {
	void *ret;

	libc_mutex_lock(&libkernel_heap_lock, -1);

	if((libkernel_heap_current + size) > LIBKERNEL_HEAP_SIZE) {
		libc_mutex_unlock(&libkernel_heap_lock);
		return NULL;
	}

	ret = &libkernel_heap[libkernel_heap_current];
	libkernel_heap_current += size;
	libc_mutex_unlock(&libkernel_heap_lock);
	return ret;
}

/** Free memory previously allocated with malloc().
 * @param addr		Address allocated. */
void free(void *addr) {
	/* Nothing happens. It probably should sometime. TODO. */
}
