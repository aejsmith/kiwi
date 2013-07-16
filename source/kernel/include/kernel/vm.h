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
 * @brief		Virtual memory management.
 */

#ifndef __KERNEL_VM_H
#define __KERNEL_VM_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Address specification for kern_vm_map(). */
#define VM_ADDRESS_ANY		1	/**< Place at any address. */
#define VM_ADDRESS_EXACT	2	/**< Place at exactly the address specified. */

/** Mapping protection flags. */
#define VM_PROT_READ		(1<<0)	/**< Mapping should be readable. */
#define VM_PROT_WRITE		(1<<1)	/**< Mapping should be writable. */
#define VM_PROT_EXECUTE		(1<<2)	/**< Mapping should be executable. */

/** Behaviour flags for kern_vm_map(). */
#define VM_MAP_PRIVATE		(1<<0)	/**< Modifications should not be visible to other processes. */
#define VM_MAP_STACK		(1<<1)	/**< Mapping contains a stack and should have a guard page. */
#define VM_MAP_OVERCOMMIT	(1<<2)	/**< Allow overcommitting of memory. */
#define VM_MAP_INHERIT		(1<<3)	/**< Region will be duplicated to child processes. */

extern status_t kern_vm_map(void **addrp, size_t size, unsigned spec,
	uint32_t protection, uint32_t flags, handle_t handle, offset_t offset,
	const char *name);
extern status_t kern_vm_unmap(void *start, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_VM_H */
