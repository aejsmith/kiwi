/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 address space layout definitions.
 *
 * This file contains definitions for the virtual address space layout. The
 * layout on AMD64 is as follows:
 *
 *  0x0000000000000000-0x00007fffffffffff - 128TB - User address space.
 *   ... non-canonical address space ...
 *  0xffff800000000000-0xfffffeffffffffff - 127TB - Physical map area.
 *  0xffffff0000000000-0xffffff7fffffffff - 512GB - Page database.
 *  0xffffff8000000000-0xffffffff7fffffff - 510GB - Kernel allocation area.
 *  0xffffffff80000000-0xffffffffffffffff - 2GB   - Kernel image/modules.
 *
 * Note that MMU context implementation currently assumes that kernel context
 * PML4 entries cannot be changed after boot, which is true with the current
 * address space layout. If address space layout changes such that this is no
 * longer the case (e.g. some regions are expanded to take more than 1 PML4
 * entry), we will need to account for this.
 */

#pragma once

/** Memory layout definitions. */
#define USER_BASE           0x0000000000000000  /**< User address space base. */
#define USER_SIZE           0x0000800000000000  /**< User address space size (128TB). */
#define USER_ANY_BASE       0x0000000100000000  /**< Search base for VM_ADDRESS_ANY. */
#define KERNEL_BASE         0xffff800000000000  /**< Kernel address space base. */
#define KERNEL_SIZE         0x0000800000000000  /**< Kernel address space size (128TB). */
#define KERNEL_PMAP_BASE    0xffff800000000000  /**< Physical map area base. */
#define KERNEL_PMAP_SIZE    0x00007f0000000000  /**< Physical map area size (127TB). */
#define KERNEL_PMAP_OFFSET  0x0000000000000000  /**< Physical map area offset. */
#define KERNEL_PDB_BASE     0xffffff0000000000  /**< Page database base. */
#define KERNEL_PDB_SIZE     0x0000008000000000  /**< Page database size (512GB). */
#define KERNEL_KMEM_BASE    0xffffff8000000000  /**< Kernel allocation area base. */
#define KERNEL_KMEM_SIZE    0x0000007f80000000  /**< Kernel allocation area size (510GB). */
#define KERNEL_VIRT_BASE    0xffffffff80000000  /**< Kernel virtual base address. */
#define KERNEL_MODULE_BASE  0xffffffffc0000000  /**< Module area base. */
#define KERNEL_MODULE_SIZE  0x0000000040000000  /**< Module area size (1GB). */

#ifndef __ASM__
extern char __text_seg_start[], __text_seg_end[];
extern char __data_seg_start[], __data_seg_end[];
extern char __init_seg_start[], __init_seg_end[];
extern char __ap_trampoline_start[], __ap_trampoline_end[];
#endif
