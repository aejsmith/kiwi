/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		ID allocator.
 */

#include <lib/id_alloc.h>

#include <assert.h>

/** Allocate a new ID.
 * @param alloc		Allocator to allocate from.
 * @return		New ID, or -1 if no IDs available. */
int32_t id_alloc_get(id_alloc_t *alloc) {
	int id;

	mutex_lock(&alloc->lock);

	/* Find a handle ID in the table. */
	id = bitmap_ffz(&alloc->bitmap);
	if(id < 0) {
		mutex_unlock(&alloc->lock);
		return -1;
	}

	bitmap_set(&alloc->bitmap, id);
	mutex_unlock(&alloc->lock);
	return id;
}

/** Free a previously allocated ID.
 * @param alloc		Allocator to free to.
 * @param id		ID to free. */
void id_alloc_release(id_alloc_t *alloc, int32_t id) {
	mutex_lock(&alloc->lock);

	assert(bitmap_test(&alloc->bitmap, id));
	bitmap_clear(&alloc->bitmap, id);

	mutex_unlock(&alloc->lock);
}

/** Reserve an ID in the allocator.
 * @param alloc		Allocator to reserve in.
 * @param id		ID to reserve. */
void id_alloc_reserve(id_alloc_t *alloc, int32_t id) {
	mutex_lock(&alloc->lock);

	assert(!bitmap_test(&alloc->bitmap, id));
	bitmap_set(&alloc->bitmap, id);

	mutex_unlock(&alloc->lock);
}

/** Initialize an ID allocator.
 * @param alloc		Allocator to initialize.
 * @param max		Highest allowed ID. */
void id_alloc_init(id_alloc_t *alloc, int32_t max) {
	mutex_init(&alloc->lock, "id_alloc_lock", 0);
	bitmap_init(&alloc->bitmap, max + 1, NULL, MM_SLEEP);
}
