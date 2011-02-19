/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Memory allocation flags.
 *
 * This file includes definitions for flags supported by all allocation
 * functions. Allocators defining their own specific flags should start
 * from bit 10.
 */

#ifndef __MM_FLAGS_H
#define __MM_FLAGS_H

/** Allocation flags supported by all allocators. */
#define MM_SLEEP		(1<<0)		/**< Block until memory is available. */
#define MM_FATAL		(1<<1)		/**< Call fatal() if unable to satisfy an allocation. */

/** Internal flags used by Vmem, defined here to include in flag mask. */
#define VM_REFILLING		(1<<2)		/**< Tag refill in progress, do not attempt to refill again. */

/** Mask to select only generic allocation flags. */
#define MM_FLAG_MASK		(MM_SLEEP | MM_FATAL | VM_REFILLING)

#endif /* __MM_FLAGS_H */
