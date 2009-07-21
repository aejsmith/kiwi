/* Kiwi address space functions
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
 * @brief		Address space functions.
 */

#include <kernel/aspace.h>

/** Structure containing arguments for __aspace_map_file(). */
typedef struct aspace_map_file_args {
	void *start;			/**< Address to map at (if not AS_REGION_FIXED). */
	size_t size;			/**< Size of area to map (multiple of page size). */
	int flags;			/**< Flags controlling the mapping. */
	handle_t handle;		/**< Handle for file to map. */
	offset_t offset;		/**< Offset in the file to map from. */
	void **addrp;			/**< Where to store address mapped to. */
} aspace_map_file_args_t;

extern int __aspace_map_file(aspace_map_file_args_t *args);

/** Map a file into memory.
 *
 * Maps part of a file into the calling process' address space. If the
 * ASPACE_MAP_FIXED flag is specified, then the region will be mapped at the
 * location specified. Otherwise, a region of unused space will be allocated
 * for the mapping. If the ASPACE_MAP_PRIVATE flag is specified, then changes
 * made to the mapped data will not be made in the underlying file, and will
 * not be visible to other regions mapping the file. Also, changes made to the
 * file's data after the mapping has been accessing it may not be visible in
 * the mapping. If the ASPACE_MAP_PRIVATE flag is not specified, then changes
 * to the mapped data will be made in the underlying file, and will be visible
 * to other regions mapping the file.
 *
 * @param start		Start address of region (if ASPACE_MAP_FIXED).
 * @param size		Size of region to map (multiple of page size).
 * @param flags		Flags to control mapping behaviour (ASPACE_MAP_*).
 * @param handle	Handle for file to map.
 * @param offset	Offset in the file to start mapping at (multiple of
 *			page size).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_map_file(void *start, size_t size, int flags, handle_t handle, offset_t offset, void **addrp) {
	aspace_map_file_args_t args;

	args.start = start;
	args.size = size;
	args.flags = flags;
	args.handle = handle;
	args.offset = offset;
	args.addrp = addrp;

	return __aspace_map_file(&args);
}
