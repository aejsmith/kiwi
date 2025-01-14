/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory management core definitions.
 */

#pragma once

/** Memory allocation behaviour flags. */
enum {
    MM_NOWAIT           = 0,        /**< Attempt to reclaim memory, but don't wait if none available. */
    MM_WAIT             = (1<<0),   /**< Block until memory is available, guarantees success. */
    MM_ATOMIC           = (1<<1),   /**< Does not wait for memory, does not attempt reclaim. */
    MM_BOOT             = (1<<2),   /**< Allocation required for boot, must succeed. */
    MM_ZERO             = (1<<3),   /**< Zero the allocated memory. */

    /**
     * These flags are the default set of allocation behaviour flags to be used
     * by kernel allocations when no special behaviour is required. MM_WAIT is
     * included in these flags, thus ensuring that allocations will succeed.
     * This is good for most reasonably-sized allocations, but for very large
     * allocations, consider MM_KERNEL_NOWAIT. Furthermore, see the
     * documentation for MM_USER.
     */
    MM_KERNEL           = MM_WAIT,

    /**
     * These flags are the set of allocation behaviour flags to use for kernel
     * allocations which should not wait for memory to be free, and are
     * therefore allowed to fail. This should be used for large kernel
     * allocations. They are otherwise the same as MM_KERNEL.
     */
    MM_KERNEL_NOWAIT    = MM_NOWAIT,

    /**
     * These flags should be used whenever allocating buffers to copy data from
     * userspace into, especially when the size is specified by userspace.
     * MM_NOWAIT is included, therefore allocations with this can fail and must
     * be checked for failure. This is to prevent userspace from specifying an
     * unreasonable allocation size and hanging itself in uninterruptible sleep
     * waiting for memory that it won't ever be able to get.
     */
    MM_USER             = MM_NOWAIT,

    /** Mask to select only generic allocation flags. */
    MM_FLAG_MASK        = MM_WAIT | MM_ATOMIC | MM_BOOT,
};
