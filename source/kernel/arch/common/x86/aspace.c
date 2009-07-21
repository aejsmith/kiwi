/* Kiwi x86 address space functions
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
 * @brief		x86 address space functions.
 */

#include <mm/aspace.h>

/** X86 address space creation function.
 *
 * Marks certain regions as reserved in a new address space, such as the
 * page at 0x0.
 *
 * @param as		Address space being created.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_arch_create(aspace_t *as) {
	int ret;

	if((ret = aspace_reserve(as, 0x0, PAGE_SIZE)) != 0) {
		return ret;
	}

	return 0;
}
