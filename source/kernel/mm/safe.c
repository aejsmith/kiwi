/*
 * Copyright (C) 2009-2013 Alex Smith
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

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <setjmp.h>
#include <status.h>

/** Common entry code for userspace memory functions. */
#define usermem_enter() \
    if (setjmp(curr_thread->usermem_context) != 0) \
        return STATUS_INVALID_ADDR; \
    \
    curr_thread->in_usermem = true; \
    compiler_barrier()

/** Common exit code for userspace memory functions. */
#define usermem_exit() \
    compiler_barrier(); \
    curr_thread->in_usermem = false

/** Code to check parameters execute a statement. */
#define usermem_wrap(addr, count, stmt) \
    if (!is_user_range(addr, count)) \
        return STATUS_INVALID_ADDR; \
    usermem_enter(); \
    stmt; \
    usermem_exit(); \
    return STATUS_SUCCESS

/** Copy data from user memory.
 * @param dest          The kernel memory area to copy to.
 * @param src           The user memory area to copy from.
 * @param count         The number of bytes to copy.
 * @return              STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *                      failure. */
status_t memcpy_from_user(void *dest, const void *src, size_t count) {
    usermem_wrap(src, count, memcpy(dest, src, count));
}

/** Copy data to user memory.
 * @param dest          The user memory area to copy to.
 * @param src           The kernel memory area to copy from.
 * @param count         The number of bytes to copy.
 * @return              STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *                      failure. */
status_t memcpy_to_user(void *dest, const void *src, size_t count) {
    usermem_wrap(dest, count, memcpy(dest, src, count));
}

/** Fill a user memory area.
 * @param dest          The user memory area to fill.
 * @param val           The value to fill with.
 * @param count         The number of bytes to fill.
 * @return              STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *                      failure. */
status_t memset_user(void *dest, int val, size_t count) {
    usermem_wrap(dest, count, memset(dest, val, count));
}

/** Get the length of a user string.
 * @param str           Pointer to the string.
 * @param _len          Where to store string length.
 * @return              STATUS_SUCCESS on success, STATUS_INVALID_ADDR on
 *                      failure. */
status_t strlen_user(const char *str, size_t *_len) {
    size_t retval = 0;

    usermem_enter();

    while (true) {
        if (!is_user_range(str, retval + 1)) {
            usermem_exit();
            return STATUS_INVALID_ADDR;
        } else if (str[retval] == 0) {
            break;
        }

        retval++;
    }

    *_len = retval;
    usermem_exit();
    return STATUS_SUCCESS;
}

/**
 * Duplicate a string from user memory.
 *
 * Allocates a buffer large enough and copies across a string from user memory.
 * The allocation is not made using MM_WAIT, as there is no length limit and
 * therefore the length could be too large to fit in memory. Use of
 * strndup_from_user() is preferred to this.
 *
 * @param src           Location to copy from.
 * @param _dest         Pointer to location in which to store address of
 *                      destination buffer.
 *
 * @return              Status code describing result of the operation.
 *                      Returns STATUS_INVALID_ARG if the string is
 *                      zero-length.
 */
status_t strdup_from_user(const void *src, char **_dest) {
    status_t ret;
    size_t len;
    char *d;

    ret = strlen_user(src, &len);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (len == 0) {
        return STATUS_INVALID_ARG;
    }

    d = kmalloc(len + 1, MM_USER);
    if (!d)
        return STATUS_NO_MEMORY;

    ret = memcpy_from_user(d, src, len);
    if (ret != STATUS_SUCCESS) {
        kfree(d);
        return ret;
    }

    d[len] = 0;

    *_dest = d;
    return STATUS_SUCCESS;
}

/**
 * Duplicate a string from user memory.
 *
 * Allocates a buffer large enough and copies across a string from user memory.
 * If the string is longer than the maximum length, then an error will be
 * returned. Because a length limit is provided, the allocation is made using
 * MM_WAIT - it is assumed that the limit is sensible.
 *
 * @param src           Location to copy from.
 * @param max           Maximum length allowed.
 * @param _dest         Pointer to location in which to store address of
 *                      destination buffer.
 *
 * @return              Status code describing result of the operation.
 *                      Returns STATUS_INVALID_ARG if the string is
 *                      zero-length.
 */
status_t strndup_from_user(const void *src, size_t max, char **_dest) {
    status_t ret;
    size_t len;
    char *d;

    ret = strlen_user(src, &len);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (len == 0) {
        return STATUS_INVALID_ARG;
    } else if (len > max) {
        return STATUS_TOO_LONG;
    }

    d = kmalloc(len + 1, MM_KERNEL);
    ret = memcpy_from_user(d, src, len);
    if (ret != STATUS_SUCCESS) {
        kfree(d);
        return ret;
    }

    d[len] = 0;

    *_dest = d;
    return STATUS_SUCCESS;
}

/**
 * Copy a NULL-terminated array of strings from user memory.
 *
 * Copies a NULL-terminated array of strings from user memory. The array
 * itself and each array entry must be freed with kfree() once no longer
 * needed.
 *
 * @param src           Array to copy.
 * @param _array        Pointer to location in which to store address of
 *                      allocated array.
 *
 * @return              Status code describing result of the operation.
 */
status_t arrcpy_from_user(const char *const src[], char ***_array) {
    char **array = NULL, **narr;
    status_t ret;
    int i;

    /* Copy the arrays across. */
    for (i = 0; ; i++) {
        narr = krealloc(array, sizeof(char *) * (i + 1), MM_USER);
        if (!narr) {
            ret = STATUS_NO_MEMORY;
            goto fail;
        }

        array = narr;
        array[i] = NULL;

        ret = memcpy_from_user(&array[i], &src[i], sizeof(char *));
        if (ret != STATUS_SUCCESS) {
            array[i] = NULL;
            goto fail;
        } else if (!array[i]) {
            break;
        }

        ret = strdup_from_user(array[i], &array[i]);
        if (ret != STATUS_SUCCESS) {
            array[i] = NULL;
            goto fail;
        }
    }

    *_array = array;
    return STATUS_SUCCESS;

fail:
    if (array) {
        for (i = 0; array[i]; i++)
            kfree(array[i]);

        kfree(array);
    }

    return ret;
}

#define BUILD_READ(width) \
    status_t __read_user##width(const void *_ptr, void *_dest) { \
        const uint##width##_t *ptr = _ptr; \
        uint##width##_t *dest = _dest; \
        usermem_wrap(ptr, 4, *dest = *ptr); \
    }

BUILD_READ(64);
BUILD_READ(32);
BUILD_READ(16);
BUILD_READ(8);

#define BUILD_WRITE(width) \
    status_t __write_user##width(void *_ptr, const void *_src) { \
        uint##width##_t *ptr = _ptr; \
        const uint##width##_t *src = _src; \
        usermem_wrap(ptr, 4, *ptr = *src); \
    }

BUILD_WRITE(64);
BUILD_WRITE(32);
BUILD_WRITE(16);
BUILD_WRITE(8);
