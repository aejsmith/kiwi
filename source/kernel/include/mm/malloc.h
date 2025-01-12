/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
