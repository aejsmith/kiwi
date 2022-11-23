/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Unidirectional data pipe implementation.
 */

#pragma once

#include <io/file.h>

#include <kernel/pipe.h>

#include <lib/notifier.h>

#include <sync/mutex.h>
#include <sync/condvar.h>

struct io_request;

/** Size of a pipe's data buffer. */
#define PIPE_SIZE       PAGE_SIZE

/** Structure containing a pipe. */
typedef struct pipe {
    file_t file;                    /**< File header. */

    mutex_t lock;                   /**< Lock to protect pipe. */

    bool read_open : 1;             /**< Whether the read end is open. */
    bool write_open : 1;            /**< Whether the write end is open. */

    condvar_t space_cvar;           /**< Condition to wait for space on. */
    condvar_t data_cvar;            /**< Condition to wait for data on. */

    notifier_t space_notifier;      /**< Notifier for space availability. */
    notifier_t data_notifier;       /**< Notifier for data availability. */

    uint8_t *buf;                   /**< Circular data buffer. */
    size_t start;                   /**< Start position of buffer. */
    size_t count;                   /**< Number of bytes in buffer. */

    uint32_t id;                    /**< Pipe ID (for debugging purposes). */
} pipe_t;

extern status_t pipe_io(pipe_t *pipe, struct io_request *request, bool nonblock);
extern void pipe_wait(pipe_t *pipe, bool write, object_event_t *event);
extern void pipe_unwait(pipe_t *pipe, bool write, object_event_t *event);

extern pipe_t *pipe_create(unsigned mmflag);
extern void pipe_destroy(pipe_t *pipe);
