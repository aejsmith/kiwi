/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Physical memory management.
 */

#pragma once

#include <arch/page.h>

#include <mm/mm.h>

struct device;

extern void *phys_map(phys_ptr_t addr, size_t size, unsigned mmflag);
extern void phys_unmap(void *addr, size_t size, bool shared);

extern void *device_phys_map(struct device *device, phys_ptr_t addr, size_t size, unsigned mmflag);

extern status_t phys_alloc(
    phys_size_t size, phys_ptr_t align, phys_ptr_t boundary, phys_ptr_t minaddr,
    phys_ptr_t maxaddr, unsigned mmflag, phys_ptr_t *_base);
extern void phys_free(phys_ptr_t base, phys_size_t size);
extern bool phys_copy(phys_ptr_t dest, phys_ptr_t source, unsigned mmflag);
