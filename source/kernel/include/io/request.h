/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               I/O request API.
 */

#pragma once

#include <kernel/file.h>

#include <proc/thread.h>

/** Types of I/O operation. */
typedef enum io_op {
    IO_OP_READ,                     /**< Read from the file. */
    IO_OP_WRITE,                    /**< Write to the file. */
} io_op_t;

/** Target address space for an I/O operation. */
typedef enum io_target {
    IO_TARGET_KERNEL,               /**< Buffer is in kernel address space. */
    IO_TARGET_USER,                 /**< Buffer is in user address space. */
} io_target_t;

/** Structure containing information for an I/O request. */
typedef struct io_request {
    io_vec_t *vecs;                 /**< I/O vectors. */
    size_t count;                   /**< Number of I/O vectors. */
    offset_t offset;                /**< Offset in the object. */
    size_t total;                   /**< Total size to be transferred. */
    size_t transferred;             /**< Number of bytes transferred so far. */
    io_op_t op;                     /**< Operation to perform. */
    io_target_t target;             /**< Target address space. */
    thread_t *thread;               /**< Thread performing the request. */
} io_request_t;

extern status_t io_request_init(
    io_request_t *request, const io_vec_t *vecs, size_t count, offset_t offset,
    io_op_t op, io_target_t target);
extern void io_request_destroy(io_request_t *request);

extern status_t io_request_copy(io_request_t *request, void *buf, size_t size);
extern void *io_request_map(io_request_t *request, size_t size);
