/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Memory area functions.
 */

#ifndef __KERNEL_AREA_H
#define __KERNEL_AREA_H

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Access rights for area objects. */
#define AREA_RIGHT_READ		(1<<0)	/**< Allow mapping for reading. */
#define AREA_RIGHT_WRITE	(1<<1)	/**< Allow mapping for writing. */

extern status_t kern_area_create(size_t size, handle_t source, offset_t offset,
                                 const object_security_t *security,
                                 object_rights_t rights, handle_t *handlep);
extern status_t kern_area_open(area_id_t id, object_rights_t rights, handle_t *handlep);
extern area_id_t kern_area_id(handle_t handle);
extern size_t kern_area_size(handle_t handle);
extern status_t kern_area_resize(handle_t handle, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_AREA_H */
