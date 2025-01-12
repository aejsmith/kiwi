/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Object ID allocator.
 */

#pragma once

#include <sync/spinlock.h>

/** ID allocator structure. */
typedef struct id_allocator {
    spinlock_t lock;            /**< Lock to protect the allocator. */
    unsigned long *bitmap;      /**< Bitmap of IDs. */
    size_t nbits;               /**< Number of bits in the bitmap. */
} id_allocator_t;

extern int32_t id_allocator_alloc(id_allocator_t *alloc);
extern void id_allocator_free(id_allocator_t *alloc, int32_t id);
extern void id_allocator_reserve(id_allocator_t *alloc, int32_t id);

extern status_t id_allocator_init(id_allocator_t *alloc, int32_t max, uint32_t mmflag);
extern void id_allocator_destroy(id_allocator_t *alloc);
