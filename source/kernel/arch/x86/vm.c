/* Kiwi x86 virtual memory manager functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		x86 virtual memory manager functions.
 */

#include "../../generic/mm/vm/vm_priv.h"

/** X86-specific address space initialisation function.
 *
 * Marks certain regions as reserved in a new address space, such as the
 * page at 0x0.
 *
 * @param as		Address space being created.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_aspace_arch_init(vm_aspace_t *as) {
	return vm_reserve(as, 0x0, PAGE_SIZE);
}
