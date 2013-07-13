/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		I/O request API.
 *
 * @todo		Should we lock the target buffer into memory so that we
 *			don't page fault trying to access it? This could cause
 *			problems: if a fault occurs while some driver is trying
 *			to access the buffer, and that fault causes the driver
 *			to be reentered, we could get locking crashes.
 *			Alternatively we could just say that you should ensure
 *			that it is safe to reenter the driver when performing
 *			a copy.
 */

#include <io/request.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/thread.h>

#include <status.h>

/** Initialize an I/O request.
 * @param request	Request to initialize.
 * @param vecs		I/O vectors.
 * @param count		Number of I/O vectors.
 * @param offset	Offset to perform I/O at.
 * @param op		I/O operation to perform.
 * @param target	Target address space. If set to IO_TARGET_USER, the
 *			target will be the current thread's address space.
 * @return		Status code describing result of the operation. */
status_t io_request_init(io_request_t *request, const io_vec_t *vecs, size_t count,
	offset_t offset, io_op_t op, io_target_t target)
{
	status_t ret;
	size_t i;

	request->offset = offset;
	request->total = 0;
	request->op = op;
	request->target = target;
	request->thread = curr_thread;

	/* Validate and copy I/O vectors. Remove entries whose count is 0. */
	request->vecs = kmalloc(sizeof(*request->vecs) * count, MM_KERNEL);
	request->count = 0;
	for(i = 0; i < count; i++) {
		if(!vecs[i].size)
			continue;

		/* Validate addresses on user address spaces. */
		if(target == IO_TARGET_USER) {
			ret = validate_user_address(vecs[i].buffer, vecs[i].size);
			if(ret != STATUS_SUCCESS) {
				kfree(request->vecs);
				return ret;
			}
		}

		request->vecs[request->count].buffer = vecs[i].buffer;
		request->vecs[request->count].size = vecs[i].size;
		request->count++;
	}

	return STATUS_SUCCESS;
}

/** Destroy an I/O request.
 * @param request	Request to destroy. */
void io_request_destroy(io_request_t *request) {
	kfree(request->vecs);
}
