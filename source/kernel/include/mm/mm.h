/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Memory management core definitions.
 */

#pragma once

/** Memory allocation behaviour flags. */
#define MM_NOWAIT       0       /**< Attempt to reclaim memory, but don't wait if none available. */
#define MM_WAIT         (1<<0)  /**< Block until memory is available, guarantees success. */
#define MM_ATOMIC       (1<<1)  /**< Does not wait for memory, does not attempt reclaim. */
#define MM_BOOT         (1<<2)  /**< Allocation required for boot, must succeed. */
#define MM_ZERO         (1<<3)  /**< Zero the allocated memory. */

/**
 * Default allocation flags for kernel allocations.
 *
 * These flags are the default set of allocation behaviour flags to be used by
 * kernel allocations when no special behaviour is required. MM_WAIT is
 * included in these flags, thus ensuring that allocations will succeed. This
 * is good for most reasonably-sized allocations, but for very large
 * allocations, consider removing MM_WAIT. Furthermore, see the documentation
 * for MM_USER.
 */
#define MM_KERNEL       (MM_WAIT)

/**
 * Default allocation flags for userspace-related allocations.
 *
 * These flags should be used whenever allocating buffers to copy data from
 * userspace into, especially when the size is specified by userspace.
 * MM_NOWAIT is included, therefore allocations with this can fail and must be
 * checked for failure. This is to prevent userspace from specifying an
 * unreasonable allocation size and hanging itself in uninterruptible sleep
 * waiting for memory that it won't ever be able to get.
 */
#define MM_USER         (MM_NOWAIT)

/** Mask to select only generic allocation flags. */
#define MM_FLAG_MASK    (MM_WAIT | MM_ATOMIC | MM_BOOT)
