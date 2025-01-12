/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Object ID allocator.
 */

#include <lib/bitmap.h>
#include <lib/id_allocator.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/** Allocate a new ID.
 * @param alloc         Allocator to allocate from.
 * @return              New ID, or -1 if no IDs available. */
int32_t id_allocator_alloc(id_allocator_t *alloc) {
    spinlock_lock(&alloc->lock);

    int32_t id = bitmap_ffz(alloc->bitmap, alloc->nbits);
    if (id < 0) {
        spinlock_unlock(&alloc->lock);
        return -1;
    }

    bitmap_set(alloc->bitmap, id);

    spinlock_unlock(&alloc->lock);
    return id;
}

/** Free a previously allocated ID.
 * @param alloc         Allocator to free to.
 * @param id            ID to free. */
void id_allocator_free(id_allocator_t *alloc, int32_t id) {
    spinlock_lock(&alloc->lock);

    assert(bitmap_test(alloc->bitmap, id));
    bitmap_clear(alloc->bitmap, id);

    spinlock_unlock(&alloc->lock);
}

/** Reserve an ID in the allocator.
 * @param alloc         Allocator to reserve in.
 * @param id            ID to reserve. */
void id_allocator_reserve(id_allocator_t *alloc, int32_t id) {
    spinlock_lock(&alloc->lock);

    assert(!bitmap_test(alloc->bitmap, id));
    bitmap_set(alloc->bitmap, id);

    spinlock_unlock(&alloc->lock);
}

/** Initialise an ID allocator.
 * @param alloc         Allocator to initialise.
 * @param max           Highest allowed ID.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing the result of the operation. */
status_t id_allocator_init(id_allocator_t *alloc, int32_t max, uint32_t mmflag) {
    spinlock_init(&alloc->lock, "id_allocator_lock");

    alloc->nbits  = max + 1;
    alloc->bitmap = bitmap_alloc(alloc->nbits, mmflag);

    return (alloc->bitmap) ? STATUS_SUCCESS : STATUS_NO_MEMORY;
}

/** Destroy an ID allocator.
 * @param alloc         Allocator to destroy. */
void id_allocator_destroy(id_allocator_t *alloc) {
    kfree(alloc->bitmap);
}
