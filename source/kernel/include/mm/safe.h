/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Safe user memory access functions.
 */

#ifndef __MM_SAFE_H
#define __MM_SAFE_H

#include <mm/aspace.h>
#include <mm/mm.h>

#include <assert.h>
#include <types.h>

#if USER_BASE > 0

/** Check whether an address is a user address.
 * @param addr          Address to check.
 * @return              Whether the address is a user address. */
static inline bool is_user_address(const void *addr) {
    return ((ptr_t)addr >= USER_BASE && (ptr_t)addr <= USER_END);
}

/** Check if an address range is within userspace memory.
 * @param addr          Base address.
 * @param size          Size of range.
 * @return              Whether the range is completely in user memory. */
static inline bool is_user_range(const void *addr, size_t size) {
    if (!size)
        size = 1;

    return
        ((ptr_t)addr >= USER_BASE &&
        (ptr_t)addr + size - 1 <= USER_END &&
        (ptr_t)addr + size - 1 >= (ptr_t)addr);
}

#else /* USER_BASE > 0 */

static inline bool is_user_address(const void *addr) {
    return ((ptr_t)addr < USER_SIZE);
}

static inline bool is_user_range(const void *addr, size_t size) {
    if (!size)
        size = 1;

    return ((ptr_t)addr + size - 1 <= USER_END && (ptr_t)addr + size -1 >= (ptr_t)addr);
}

#endif /* USER_BASE > 0 */

extern status_t memcpy_from_user(void *dest, const void *src, size_t count);
extern status_t memcpy_to_user(void *dest, const void *src, size_t count);
extern status_t memset_user(void *dest, int val, size_t count);
extern status_t strlen_user(const char *str, size_t *_len);

extern status_t strdup_from_user(const void *src, char **_dest);
extern status_t strndup_from_user(const void *src, size_t max, char **_dest);
extern status_t arrcpy_from_user(const char *const src[], char ***_array);

/** Read a value from userspace.
 * @param ptr           User pointer to read from, must point to a simple type.
 * @param val           Pointer to location in which to store result.
 * @return              Status code describing result of the operation. */
#define read_user(ptr, val) \
    __extension__ \
    ({ \
        status_t __ret; \
        static_assert(sizeof(*(ptr)) == 8 || \
                sizeof(*(ptr)) == 4 || \
                sizeof(*(ptr)) == 2 || \
                sizeof(*(ptr)) == 1, \
            "Unsupported value size"); \
        switch (sizeof(*(ptr))) { \
        case 8: \
            __ret = __read_user64((void *)(ptr), (void *)(val)); \
            break; \
        case 4: \
            __ret = __read_user32((void *)(ptr), (void *)(val)); \
            break; \
        case 2: \
            __ret = __read_user16((void *)(ptr), (void *)(val)); \
            break; \
        case 1: \
            __ret = __read_user8((void *)(ptr), (void *)(val)); \
            break; \
        } \
        __ret; \
    })

/** Write a value to userspace.
 * @param ptr           User pointer to write to, must point to a simple type.
 * @param val           Value to write (only evaluated once).
 * @return              Status code describing result of the operation. */
#define write_user(ptr, val)    \
    __extension__ \
    ({ \
        status_t __ret; \
        typeof(*(ptr)) __val = (val); \
        static_assert(sizeof(*(ptr)) == 8 || \
                sizeof(*(ptr)) == 4 || \
                sizeof(*(ptr)) == 2 || \
                sizeof(*(ptr)) == 1, \
            "Unsupported value size"); \
        switch (sizeof(*(ptr))) { \
        case 8: \
            __ret = __write_user64((void *)(ptr), (void *)&__val); \
            break; \
        case 4: \
            __ret = __write_user32((void *)(ptr), (void *)&__val); \
            break; \
        case 2: \
            __ret = __write_user16((void *)(ptr), (void *)&__val); \
            break; \
        case 1: \
            __ret = __write_user8((void *)(ptr), (void *)&__val); \
            break; \
        } \
        __ret; \
    })

extern status_t __read_user64(const void *ptr, void *dest);
extern status_t __read_user32(const void *ptr, void *dest);
extern status_t __read_user16(const void *ptr, void *dest);
extern status_t __read_user8(const void *ptr, void *dest);

extern status_t __write_user64(void *ptr, const void *src);
extern status_t __write_user32(void *ptr, const void *src);
extern status_t __write_user16(void *ptr, const void *src);
extern status_t __write_user8(void *ptr, const void *src);

#endif /* __MM_SAFE_H */
