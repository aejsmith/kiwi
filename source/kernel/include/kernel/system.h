/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
