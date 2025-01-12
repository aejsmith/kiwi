/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Fixed heap allocator.
 */

#pragma once

#include <types.h>

struct fixed_heap_tag;

/** Structure containing a fixed heap allocator. */
typedef struct fixed_heap {
    struct fixed_heap_tag *tags;        /**< List of tags. */
} fixed_heap_t;

extern void *fixed_heap_alloc(fixed_heap_t *heap, size_t size);
extern void fixed_heap_free(fixed_heap_t *heap, void *ptr);

extern void fixed_heap_init(fixed_heap_t *heap, void *mem, size_t size);
