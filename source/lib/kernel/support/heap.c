/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kernel library heap functions.
 */

//#include <util/mutex.h>
#include <stdlib.h>
#include "../libkernel.h"

/** Statically allocated heap. */
static uint8_t libkernel_heap[LIBKERNEL_HEAP_SIZE];
static size_t libkernel_heap_current = 0;

/** Heap operations. */
static libkernel_heap_ops_t *libkernel_heap_ops = NULL;

/** Lock to protect the heap. */
//static LIBC_MUTEX_DECLARE(libkernel_heap_lock);

/** Allocate some memory.
 * @param size		Size to allocate.
 * @return		Pointer to allocation on success, NULL on failure. */
void *malloc(size_t size) {
	void *ret;

	//libc_mutex_lock(&libkernel_heap_lock, -1);

	if(libkernel_heap_ops) {
		ret = libkernel_heap_ops->alloc(size);
	} else if((libkernel_heap_current + size) > LIBKERNEL_HEAP_SIZE) {
		ret = NULL;
	} else {
		ret = &libkernel_heap[libkernel_heap_current];
		libkernel_heap_current += size;
	}

	//libc_mutex_unlock(&libkernel_heap_lock);
	return ret;
}

/** Change the size of an allocation.
 * @param addr		Allocation to resize.
 * @param size		New size for the allocation.
 * @return		Pointer to allocation on success, NULL on failure. */
void *realloc(void *addr, size_t size) {
	/* This is not supported on the early heap. */
	return libkernel_heap_ops->realloc(addr, size);
}

/** Free memory previously allocated with malloc().
 * @param addr		Address allocated. */
void free(void *addr) {
	if((ptr_t)addr >= (ptr_t)libkernel_heap && (ptr_t)addr < ((ptr_t)libkernel_heap + LIBKERNEL_HEAP_SIZE)) {
		return;
	} else if(libkernel_heap_ops) {
		libkernel_heap_ops->free(addr);
	}
}

/** Set the kernel library heap operations.
 * @param ops		Operations to use. */
__export void libkernel_heap_configure(libkernel_heap_ops_t *ops) {
	libkernel_heap_ops = ops;
}
