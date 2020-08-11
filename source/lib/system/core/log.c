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
