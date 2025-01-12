/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Logging functions.
 */

#include <core/log.h>

#include <stdio.h>

#include "libsystem.h"

/** Write a log message.
 * @param level         Log level.
 * @param fmt           Message format string.
 * @param args          Format arguments. */
void core_log_args(core_log_level_t level, const char *fmt, va_list args) {
    FILE *stream = (level >= CORE_LOG_ERROR) ? stderr : stdout;

    /* Just write to the console for now. */
    fprintf(stream, "%s: ", __program_name);
    vfprintf(stream, fmt, args);
    fprintf(stream, "\n");
}

/** Write a log message.
 * @param level         Log level.
 * @param fmt           Message format string.
 * @param ...           Format arguments. */
void core_log(core_log_level_t level, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    core_log_args(level, fmt, args);
    va_end(args);
}
