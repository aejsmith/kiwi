/*
 * Copyright (C) 2009-2021 Alex Smithh
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
 * @brief               Dynamic array implementation.
 */

#pragma once

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>

/** Dynamic array structure. */
typedef struct array {
    void *data;
    size_t count;
} array_t;

/** Initializes a statically defined array. */
#define ARRAY_INITIALIZER(_var) {}

/** Statically defines a dynamic array. */
#define ARRAY_DEFINE(_var) \
    array_t _var = ARRAY_INITIALIZER(_var)

/** Get a pointer to an array entry.
 * @param array         Array to get from.
 * @param type          Type of the array entries.
 * @param index         Index of entry to get.
 * @return              Pointer to the entry. */
#define array_entry(array, type, index) \
    (&((type *)(array)->data)[index])

/** Initializes a dynamic array.
 * @param array         Array to initialize. */
static inline void array_init(array_t *array) {
    array->data  = NULL;
    array->count = 0;
}

/** Clears a dynamic array.
 * @param array         Array to clear. */
static inline void array_clear(array_t *array) {
    kfree(array->data);
    array_init(array);
}

static inline void *array_insert_impl(array_t *array, size_t size, size_t index) {
    assert(index <= array->count);

    array->data = krealloc(array->data, size * (array->count + 1), MM_KERNEL);

    size_t offset = size * index;
    if (index < array->count) {
        memmove(
            (uint8_t *)array->data + offset + size,
            (uint8_t *)array->data + offset,
            size * (array->count - index));
    }

    array->count++;
    return (uint8_t *)array->data + offset;
}

/** Insert a new array entry at a given position (must be <= count).
 * @param array         Array to insert into.
 * @param type          Type of the array entries.
 * @return              Pointer to new entry. */
#define array_insert(array, type, index) \
    ((type *)array_insert_impl((array), sizeof(type), index))

static inline void *array_append_impl(array_t *array, size_t size) {
    size_t offset = size * array->count;
    array->data = krealloc(array->data, offset + size, MM_KERNEL);
    array->count++;
    return (uint8_t *)array->data + offset;
}

/** Append a new array entry.
 * @param array         Array to append to.
 * @param type          Type of the array entries.
 * @return              Pointer to new entry. */
#define array_append(array, type) \
    ((type *)array_append_impl((array), sizeof(type)))

static inline void array_remove_impl(array_t *array, size_t size, size_t index) {
    array->count--;

    if (index < array->count) {
        size_t offset = size * index;
        memmove(
            (uint8_t *)array->data + offset,
            (uint8_t *)array->data + offset + size,
            size * (array->count - index));
    }

    array->data = krealloc(array->data, size * array->count, MM_KERNEL);
}

/** Remove an array entry.
 * @param array         Array to append to.
 * @param type          Type of the array entries.
 * @param index         Index of the entry to remove. */
#define array_remove(array, type, index) \
    array_remove_impl((array), sizeof(type), (index))
