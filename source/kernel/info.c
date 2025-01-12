/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               System information functions.
 */

#include <arch/page.h>

#include <kernel/system.h>

#include <mm/safe.h>

#include <cpu.h>
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
status_t kern_system_info(uint32_t what, void *buf) {
    if (!buf)
        return STATUS_INVALID_ARG;

    switch (what) {
        case SYSTEM_INFO_PAGE_SIZE:
            return write_user((size_t *)buf, PAGE_SIZE);
        case SYSTEM_INFO_NUM_CPUS:
            return write_user((uint32_t *)buf, cpu_count);
        default:
            return STATUS_INVALID_ARG;
    }
}
