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
 * @brief		Process functions.
 */

#include <kernel/private/process.h>

/** Saved ID for the current process. */
process_id_t curr_process_id = -1;

/** Get the ID of a process.
 * @param handle	Handle for process to get ID of, or PROCESS_SELF to get
 *			ID of the calling process.
 * @return		Process ID on success, -1 if handle is invalid. */
process_id_t kern_process_id(handle_t handle) {
	/* We save the current process ID to avoid having to perform a kernel
	 * call just to get our own ID. */
	if(handle < 0) {
		return curr_process_id;
	} else {
		return _kern_process_id(handle);
	}
}
