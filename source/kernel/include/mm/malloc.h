/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Memory allocation functions.
 */

#pragma once

#include <mm/mm.h>

#include <types.h>

struct device;

extern void *kmalloc(size_t size, uint32_t mmflag) __malloc;
extern void *kcalloc(size_t nmemb, size_t size, uint32_t mmflag) __malloc;
extern void *krealloc(void *addr, size_t size, uint32_t mmflag) __malloc;
extern void kfree(void *addr);

/** Helper for __cleanup_free. */
static inline void __kfreep(void *p) {
    kfree(*(void **)p);
}

/** Attribute to free a pointer with kfree when it goes out of scope. */
#define __cleanup_kfree  __cleanup(__kfreep)

extern void *device_kmalloc(struct device *device, size_t size, uint32_t mmflag) __malloc;
extern void device_add_kalloc(struct device *device, void *addr);

extern void malloc_init(void);
