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
 * @brief               Address space layout definitions.
 */

#pragma once

#include <arch/aspace.h>

/** Last address of user address space. */
#define USER_END                (USER_BASE + USER_SIZE - 1)

/** Last address of kernel address space. */
#define KERNEL_END              (KERNEL_BASE + KERNEL_SIZE - 1)

/** Last address of physical map area. */
#define KERNEL_PMAP_END         (KERNEL_PMAP_BASE + KERNEL_PMAP_SIZE - 1)

/** Last address of page database. */
#define KERNEL_PDB_END          (KERNEL_PDB_BASE + KERNEL_PDB_SIZE - 1)

/** Last address of kmem space. */
#define KERNEL_KMEM_END         (KERNEL_KMEM_BASE + KERNEL_KMEM_SIZE - 1)

/** Last address of kernel module space. */
#ifdef KERNEL_MODULE_BASE
#   define KERNEL_MODULE_END    (KERNEL_MODULE_BASE + KERNEL_MODULE_SIZE - 1)
#endif

#ifndef __ASM__
extern char __end[];
#endif
