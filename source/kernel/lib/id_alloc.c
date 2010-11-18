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

/** Initialise an ID allocator.
 * @param alloc		Allocator to initialise.
 * @param max		Highest allowed ID. */
void id_alloc_init(id_alloc_t *alloc, int32_t max) {
	mutex_init(&alloc->lock, "id_alloc_lock", 0);
	bitmap_init(&alloc->bitmap, max + 1, NULL, MM_SLEEP);
}
