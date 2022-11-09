/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Random number generation.
 *
 * MT19937-64 implementation taken from:
 * http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/VERSIONS/C-LANG/mt19937-64.c
 *
 * TODO:
 *  - This is not suitable for cryptographic usage. Really, we should replace
 *    this eventually with something like Linux has which sources entropy from
 *    system activity and other things.
 */

#include <io/request.h>

#include <lib/random.h>
#include <lib/utility.h>

#include <device/device.h>

#include <mm/malloc.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <status.h>
#include <time.h>

#include "mt19937-64.h"

static SPINLOCK_DEFINE(random_lock);

#if CONFIG_DEBUG
static bool random_inited = false;
#endif

static inline uint64_t random_get_locked(void) {
    return genrand64_int64();
}

/** Gets a 64-bit unsigned random number (range 0, UINT64_MAX). */
uint64_t random_get_u64(void) {
    spinlock_lock(&random_lock);

    assert(random_inited);

    unsigned long ret = random_get_locked();

    spinlock_unlock(&random_lock);
    return ret;
}

/** Gets a 64-bit signed random number (range 0, INT64_MAX). */
int64_t random_get_s64(void) {
    return (int64_t)(random_get_u64() >> 1);
}

/** Gets a 32-bit unsigned random number (range 0, UINT32_MAX). */
uint32_t random_get_u32(void) {
    return (uint32_t)(random_get_u64() >> 32);
}

/** Gets a 32-bit signed random number (range 0, INT32_MAX). */
int32_t random_get_s32(void) {
    return (int32_t)(random_get_u64() >> 33);
}

__init_text void random_init(void) {
    (void)init_by_array64;

    init_genrand64(unix_time());

    #if CONFIG_DEBUG
        random_inited = true;
    #endif
}

static status_t pseudo_random_device_io(device_t *device, file_handle_t *handle, io_request_t *request) {
    if (request->op == IO_OP_WRITE)
        return STATUS_NOT_SUPPORTED;

    /* Read in chunks so we're not copying bytes at a time, but not huge chunks
     * so that we hold on to the spinlock for too long. */
    static const size_t max_chunk_size = 128;

    uint64_t *buf __cleanup_kfree = kmalloc(max_chunk_size, MM_KERNEL);

    while (request->transferred < request->total) {
        size_t remaining   = request->total - request->transferred;
        size_t chunk_size  = min(remaining, max_chunk_size);
        size_t chunk_words = round_up_pow2(chunk_size, sizeof(uint64_t)) / sizeof(uint64_t);

        spinlock_lock(&random_lock);

        for (size_t i = 0; i < chunk_words; i++)
            buf[i] = random_get_locked();

        spinlock_unlock(&random_lock);

        status_t ret = io_request_copy(request, buf, chunk_size, true);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

static const device_ops_t pseudo_random_device_ops = {
    .type = FILE_TYPE_CHAR,
    .io   = pseudo_random_device_io,
};

static void pseudo_random_device_init(void) {
    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = "pseudo_random" } },
    };

    device_t *device;
    status_t ret = device_create(
        "pseudo_random", device_virtual_dir, &pseudo_random_device_ops, NULL,
        attrs, array_size(attrs), &device);
    if (ret != STATUS_SUCCESS)
        fatal("Failed to register pseudo_random device (%d)", ret);

    device_publish(device);
}

INITCALL(pseudo_random_device_init);
