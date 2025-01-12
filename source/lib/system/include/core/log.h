/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Logging functions.
 */

#pragma once

#include <system/defs.h>

#include <inttypes.h>
#include <stdarg.h>

__SYS_EXTERN_C_BEGIN

/** Log levels. */
typedef enum core_log_level {
    CORE_LOG_DEBUG,                     /**< Debugging information. */
    CORE_LOG_NOTICE,                    /**< Informational messages. */
    CORE_LOG_WARN,                      /**< Warning messages. */
    CORE_LOG_ERROR,                     /**< Error messages. */
} core_log_level_t;

extern void core_log_args(core_log_level_t level, const char *fmt, va_list args);
extern void core_log(core_log_level_t level, const char *fmt, ...) __sys_printf(2, 3);

__SYS_EXTERN_C_END
