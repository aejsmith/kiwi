/*
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
 * @brief		Virtual memory functions.
 */

#include <kernel/vm.h>

/** Map an object into memory.
 *
 * Creates a new memory mapping within an address space that maps either an
 * object or anonymous memory. If the VM_MAP_FIXED flag is specified, then the
 * region will be mapped at the exact location specified, and any existing
 * mappings in the same region will be overwritten. Otherwise, a region of
 * unused space will be allocated for the mapping. If the VM_MAP_PRIVATE flag
 * is specified, modifications to the mapping will not be transferred through
 * to the source object, and if the address space is duplicated, the duplicate
 * and original will be given copy-on-write copies of the region. If the
 * VM_MAP_PRIVATE flag is not specified and the address space is duplicated,
 * changes made in either address space will be visible in the other.
 *
 * @param start		Start address of region (if VM_MAP_FIXED). Must be a
 *			multiple of the system page size.
 * @param size		Size of region to map. Must be a multiple of the system
 *			page size.
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param handle	Handle to object to map in. If -1, then the region
 *			will be an anonymous memory mapping.
 * @param offset	Offset into object to map from.
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_map(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp) {
	vm_map_args_t args = {
		.start = start,
		.size = size,
		.flags = flags,
		.handle = handle,
		.offset = offset,
		.addrp = addrp,
	};
	return _vm_map(&args);
}
