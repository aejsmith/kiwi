/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Memory management functions.
 */

#ifndef __BOOT_MEMORY_H
#define __BOOT_MEMORY_H

#include <arch/page.h>
#include <kargs.h>

extern void *kmalloc(size_t size);
extern void *krealloc(void *addr, size_t size);
extern void kfree(void *addr);

extern void phys_memory_add(phys_ptr_t start, phys_ptr_t end, int type);
extern void phys_memory_protect(phys_ptr_t start, phys_ptr_t end);
extern phys_ptr_t phys_memory_alloc(phys_ptr_t size, size_t align, bool reclaim);

extern void platform_memory_detect(void);
extern void memory_init(void);
extern void memory_finalise(void);

#endif /* __BOOT_MEMORY_H */
