/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Virtual memory management.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

/** Address specification for kern_vm_map(). */
#define VM_ADDRESS_ANY      1       /**< Place at any address. */
#define VM_ADDRESS_EXACT    2       /**< Place at exactly the address specified. */
#define VM_ADDRESS_HINT     3       /**< Start searching from the address specified. */

/** Mapping access flags. */
#define VM_ACCESS_READ      (1<<0)  /**< Mapping should be readable. */
#define VM_ACCESS_WRITE     (1<<1)  /**< Mapping should be writable. */
#define VM_ACCESS_EXECUTE   (1<<2)  /**< Mapping should be executable. */

/** Behaviour flags for kern_vm_map(). */
#define VM_MAP_PRIVATE      (1<<0)  /**< Modifications should not be visible to other processes. */
#define VM_MAP_STACK        (1<<1)  /**< Mapping contains a stack and should have a guard page. */
#define VM_MAP_OVERCOMMIT   (1<<2)  /**< Allow overcommitting of memory. */

extern status_t kern_vm_map(
    void **_addr, size_t size, size_t align, uint32_t spec, uint32_t access,
    uint32_t flags, handle_t handle, offset_t offset, const char *name);
extern status_t kern_vm_unmap(void *start, size_t size);

__KERNEL_EXTERN_C_END
