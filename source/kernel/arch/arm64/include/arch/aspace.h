/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 address space layout definitions.
 *
 * This file contains definitions for the virtual address space layout. We use
 * a 48-bit virtual address space, with 4KB pages (4 levels of translation
 * tables).
 *
 * The layout is as follows:
 *
 *  0x0000000000000000-0x0000ffffffffffff - 256TB - User address space.
 *   ... invalid ...
 *  0xffff000000000000-0xfffffeffffffffff - 255TB - Physical map area.
 *  0xffffff0000000000-0xffffff7fffffffff - 512GB - Page database.
 *  0xffffff8000000000-0xffffffff7fffffff - 510GB - Kernel allocation area.
 *  0xffffffff80000000-0xffffffffffffffff - 2GB   - Kernel image/modules.
 *
 * Note that kernel and modules are currently constrained to 128MB to fit within
 * the maximum +/-128MB relative branch offset. If we need to increase this we
 * will have to implement PLT support for modules.
 */

#pragma once

/** Memory layout definitions. */
#define USER_BASE           0x0000000000000000  /**< User address space base. */
#define USER_SIZE           0x0001000000000000  /**< User address space size (256TB). */
#define USER_ANY_BASE       0x0000000100000000  /**< Search base for VM_ADDRESS_ANY. */
#define KERNEL_BASE         0xffff000000000000  /**< Kernel address space base. */
#define KERNEL_SIZE         0x0001000000000000  /**< Kernel address space size (256TB). */
#define KERNEL_PMAP_BASE    0xffff000000000000  /**< Physical map area base. */
#define KERNEL_PMAP_SIZE    0x0000ff0000000000  /**< Physical map area size (255TB). */
#define KERNEL_PMAP_OFFSET  0x0000000000000000  /**< Physical map area offset. */
#define KERNEL_PDB_BASE     0xffffff0000000000  /**< Page database base. */
#define KERNEL_PDB_SIZE     0x0000008000000000  /**< Page database size (512GB). */
#define KERNEL_KMEM_BASE    0xffffff8000000000  /**< Kernel allocation area base. */
#define KERNEL_KMEM_SIZE    0x0000007f80000000  /**< Kernel allocation area size (510GB). */
#define KERNEL_VIRT_BASE    0xffffffff80000000  /**< Kernel virtual base address. */
#define KERNEL_MODULE_BASE  0xffffffff84000000  /**< Module area base. */
#define KERNEL_MODULE_SIZE  0x0000000004000000  /**< Module area size (64MB). */

#ifndef __ASM__
extern char __text_seg_start[], __text_seg_end[];
extern char __data_seg_start[], __data_seg_end[];
extern char __init_seg_start[], __init_seg_end[];
#endif
