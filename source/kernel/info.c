/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               System information functions.
 */

#include <arch/page.h>

#include <kernel/system.h>

#include <mm/safe.h>

#include <kernel.h>
#include <status.h>

/**
 * Retrieves information about the system. The what argument specifies the
 * information to get, which will be stored in the given buffer. The buffer
 * should be large enough to hold whatever information is being requested.
 *
 * @param what          Information to retrieve (see kernel/system.h).
 * @param buf           Buffer to store information in.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if what is unknown or buf is NULL.
 */
status_t kern_system_info(unsigned what, void *buf) {
    if (!buf)
        return STATUS_INVALID_ARG;

    switch (what) {
        case SYSTEM_INFO_PAGE_SIZE:
            return write_user((size_t *)buf, PAGE_SIZE);
        default:
            return STATUS_INVALID_ARG;
    }
}
