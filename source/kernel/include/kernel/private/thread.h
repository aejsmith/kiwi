/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Internal thread functions/definitions.
 */

#ifndef __KERNEL_PRIVATE_THREAD_H
#define __KERNEL_PRIVATE_THREAD_H

#include <kernel/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Actions for kern_thread_control(). */
#define THREAD_GET_TLS_ADDR	1	/**< Get TLS base address. */
#define THREAD_SET_TLS_ADDR	2	/**< Set TLS base address. */

extern status_t kern_thread_control(unsigned action, const void *in, void *out);

#ifndef KERNEL
extern status_t _kern_thread_create(const char *name, thread_entry_t *entry,
	uint32_t flags, handle_t *handlep);
extern thread_id_t _kern_thread_id(handle_t handle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PRIVATE_THREAD_H */
