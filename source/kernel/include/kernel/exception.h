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
 * @brief               Exception handling definitions.
 */

#pragma once

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct thread_context;

/** Exception information structure. */
typedef struct exception_info {
    unsigned code;                          /**< Exception code. */

    /** For memory access exceptions, the faulting address. */
    void *addr;

    union {
        /** Status code (for EXCEPTION_PAGE_ERROR). */
        status_t status;

        /** Access that occurred (for EXCEPTION_ACCESS_VIOLATION). */
        uint32_t access;
    };
} exception_info_t;

/**
 * Exception handler function type.
 *
 * Type of an exception handler function. When the exception that the handler
 * is registered for occurs, the thread's context will be saved and it will be
 * made to execute the handler. The handler receives an exception information
 * structure and a copy of the previous thread context. If the handler returns,
 * the thread will attempt to restore the context. The handler can modify the
 * context before returning.
 *
 * @param info      Exception information structure.
 * @param ctx       Thread context when the exception occurred.
 */
typedef void (*exception_handler_t)(exception_info_t *info, struct thread_context *ctx);

/** Exception codes. */
#define EXCEPTION_ADDR_UNMAPPED         1   /**< Access to non-existant memory mapping. */
#define EXCEPTION_ACCESS_VIOLATION      2   /**< Violation of mapping access flags. */
#define EXCEPTION_STACK_OVERFLOW        3   /**< Hit stack guard page. */
#define EXCEPTION_PAGE_ERROR            4   /**< Error while attempting to load a page. */
#define EXCEPTION_INVALID_ALIGNMENT     5   /**< Incorrectly aligned access. */
#define EXCEPTION_INVALID_INSTRUCTION   6   /**< Invalid instruction. */
#define EXCEPTION_INT_DIV_ZERO          7   /**< Integer division by zero. */
#define EXCEPTION_INT_OVERFLOW          8   /**< Integer overflow. */
#define EXCEPTION_FLOAT_DIV_ZERO        9   /**< Floating point division by zero. */
#define EXCEPTION_FLOAT_OVERFLOW        10  /**< Floating point overflow. */
#define EXCEPTION_FLOAT_UNDERFLOW       11  /**< Floating point underflow. */
#define EXCEPTION_FLOAT_PRECISION       12  /**< Inexact floating point result. */
#define EXCEPTION_FLOAT_DENORMAL        13  /**< Denormalized operand. */
#define EXCEPTION_FLOAT_INVALID         14  /**< Other invalid floating point operation. */
#define EXCEPTION_BREAKPOINT            15  /**< Breakpoint. */
#define EXCEPTION_ABORT                 16  /**< Software abort. */
#define EXCEPTION_MAX                   17

#ifdef __cplusplus
}
#endif
