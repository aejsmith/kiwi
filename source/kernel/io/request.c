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
 * @brief               I/O request API.
 *
 * TODO:
 *  - Should we lock the target buffer into memory so that we don't page fault
 *    trying to access it? This could cause problems: if a fault occurs while
 *    some driver is trying to access the buffer, and that fault causes the
 *    driver to be reentered, we could get locking crashes. Alternatively we
 *    could just say that you should ensure that it is safe to reenter the
 *    driver when performing a copy.
 */

#include <io/request.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <assert.h>
#include <kernel.h>
#include <status.h>

/** Initialize an I/O request.
 * @param request       Request to initialize.
 * @param vecs          I/O vectors. Will be copied, original vectors can be
 *                      freed after this function returns.
 * @param count         Number of I/O vectors.
 * @param offset        Offset to perform I/O at.
 * @param op            I/O operation to perform.
 * @param target        Target address space. If set to IO_TARGET_USER, the
 *                      target will be the current thread's address space.
 * @return              Status code describing result of the operation. */
status_t io_request_init(
    io_request_t *request, const io_vec_t *vecs, size_t count, offset_t offset,
    io_op_t op, io_target_t target)
{
    request->offset      = offset;
    request->total       = 0;
    request->transferred = 0;
    request->op          = op;
    request->target      = target;
    request->thread      = curr_thread;

    /* Validate and copy I/O vectors. Remove entries whose count is 0. */
    request->vecs  = kmalloc(sizeof(*request->vecs) * count, MM_KERNEL);
    request->count = 0;
    for (size_t i = 0; i < count; i++) {
        if (!vecs[i].size)
            continue;

        /* Validate addresses on user address spaces. */
        if (target == IO_TARGET_USER) {
            if (!is_user_range(vecs[i].buffer, vecs[i].size)) {
                kfree(request->vecs);
                return STATUS_INVALID_ADDR;
            }
        }

        request->vecs[request->count].buffer = vecs[i].buffer;
        request->vecs[request->count].size   = vecs[i].size;

        request->total += vecs[i].size;
        request->count++;
    }

    return STATUS_SUCCESS;
}

/** Destroy an I/O request.
 * @param request       Request to destroy. */
void io_request_destroy(io_request_t *request) {
    kfree(request->vecs);
}

/**
 * Copies data for an I/O request. If the request is a read, then data will be
 * copied from the supplied buffer to the request's buffer. If it is a write,
 * data will be copied from the request's buffer to the supplied buffer. The
 * data will be copied to/from after any previously copied data, and the total
 * transferred count will be updated.
 *
 * @param request       Request to copy for.
 * @param buf           Buffer to transfer data to/from.
 * @param size          Size to transfer.
 *
 * @return              Status code describing result of the operation.
 */
status_t io_request_copy(io_request_t *request, void *buf, size_t size) {
    status_t ret;

    size_t offset = 0;
    for (size_t i = 0; i < request->count && size; i++) {
        /* Find the vector to start at. */
        if (offset + request->vecs[i].size <= request->transferred) {
            offset += request->vecs[i].size;
            continue;
        }

        size_t vec_start = request->transferred - offset;
        size_t vec_size  = min(request->vecs[i].size - vec_start, size);
        void *vec_buf    = request->vecs[i].buffer + vec_start;

        if (request->op == IO_OP_WRITE) {
            /* Write, copy from the request to the supplied buffer. */
            if (request->target == IO_TARGET_USER) {
                /* FIXME: Handle different address spaces. */
                assert(request->thread == curr_thread);

                ret = memcpy_from_user(buf, vec_buf, vec_size);
                if (ret != STATUS_SUCCESS)
                    return ret;
            } else {
                memcpy(buf, vec_buf, vec_size);
            }
        } else {
            /* Read, copy to the request from the supplied buffer. */
            if (request->target == IO_TARGET_USER) {
                /* FIXME: Handle different address spaces. */
                assert(request->thread == curr_thread);

                ret = memcpy_to_user(vec_buf, buf, vec_size);
                if (ret != STATUS_SUCCESS)
                    return ret;
            } else {
                memcpy(vec_buf, buf, vec_size);
            }
        }

        request->transferred += vec_size;

        offset += request->vecs[i].size;
        buf += vec_size;
        size -= vec_size;
    }

    if (unlikely(size)) {
        fatal(
            "I/O request transfer too large (total: %zu, remaining: %zu)",
            request->total, size);
    }

    return STATUS_SUCCESS;
}
