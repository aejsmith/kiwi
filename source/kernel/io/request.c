/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
    request->flags       = 0;
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
 * data will be copied from the request's buffer to the supplied buffer.
 *
 * The data will be copied to/from at the current transfer offset (given by the
 * transferred count). If requested, the transferred count will be advanced by
 * the copy amount upon success.
 *
 * @param request       Request to copy for.
 * @param buf           Buffer to transfer data to/from.
 * @param size          Size to transfer.
 * @param advance       Whether to advance the transferred count.
 *
 * @return              Status code describing result of the operation.
 */
status_t io_request_copy(io_request_t *request, void *buf, size_t size, bool advance) {
    status_t ret;

    size_t remaining  = size;
    size_t offset     = request->transferred;
    size_t vec_offset = 0;
    for (size_t i = 0; i < request->count && remaining; i++) {
        /* Find the vector to start at. */
        if (vec_offset + request->vecs[i].size <= offset) {
            vec_offset += request->vecs[i].size;
            continue;
        }

        size_t vec_start = offset - vec_offset;
        size_t vec_size  = min(request->vecs[i].size - vec_start, remaining);
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

        vec_offset += request->vecs[i].size;
        buf += vec_size;
        remaining -= vec_size;
    }

    if (unlikely(remaining)) {
        fatal(
            "I/O request transfer too large (total: %zu, remaining: %zu)",
            request->total, remaining);
    }

    if (advance)
        request->transferred += size;

    return STATUS_SUCCESS;
}

/**
 * Tries to obtain a pointer to transfer data to/from at the current transfer
 * offset (given by the transferred count). This is possible when there is a
 * contiguous block of accessible memory of the specified size.
 *
 * If successful, the caller should transfer directly to the returned pointer.
 * If requested, the transferred count will be advanced by the specified size
 * upon success.
 *
 * If it fails, the caller must fall back to, for example, transferring to an
 * intermediate buffer and using io_request_copy().
 *
 * @param request       Request to map for.
 * @param size          Size to map.
 * @param advance       Whether to advance the transferred count.
 *
 * @return              Pointer to memory if mappable, NULL if not.
 */
void *io_request_map(io_request_t *request, size_t size, bool advance) {
    assert(size > 0);

    /* TODO: Could implement this if we could pin the userspace memory in place
     * so it is guaranteed not to fault. */
    if (request->target == IO_TARGET_USER)
        return NULL;

    if (request->transferred + size > request->total)
        return NULL;

    size_t offset = 0;
    for (size_t i = 0; i < request->count; i++) {
        if (offset + request->vecs[i].size <= request->transferred) {
            offset += request->vecs[i].size;
        } else {
            size_t vec_start = request->transferred - offset;
            size_t vec_size  = min(request->vecs[i].size - vec_start, size);

            if (vec_size == size) {
                if (advance)
                    request->transferred += size;

                return request->vecs[i].buffer + vec_start;
            } else {
                return NULL;
            }
        }
    }

    return NULL;
}
