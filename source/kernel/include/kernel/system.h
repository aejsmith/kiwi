/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Miscellaneous system functions.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

/** System information values. */
enum {
    /** System page size (size_t). */
    SYSTEM_INFO_PAGE_SIZE   = 1,

    /** Number of running CPUs (uint32_t). */
    SYSTEM_INFO_NUM_CPUS    = 2,
};

extern status_t kern_system_info(uint32_t what, void *buf);

/** Actions for kern_system_shutdown(). */
enum {
    SHUTDOWN_REBOOT         = 1,    /**< Reboot the system. */
    SHUTDOWN_POWEROFF       = 2,    /**< Power off the system. */
};

extern status_t kern_system_shutdown(uint32_t action);
extern status_t kern_system_fatal(const char *message);

__KERNEL_EXTERN_C_END
