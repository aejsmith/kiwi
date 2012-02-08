/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		KBoot utility functions.
 *
 * @warning		These functions are only available during kernel
 *			initialization. The memory they occupy is freed during
 *			boot memory reclaim.
 */

#ifndef __KERNEL_KBOOT_H
#define __KERNEL_KBOOT_H

#include "../../boot/include/kboot.h"

extern void *kboot_tag_iterate(uint32_t type, void *current);
extern void kboot_tag_release(void *current);

/** Iterate over the KBoot tag list. */
#define KBOOT_ITERATE(_type, _vtype, _vname) \
	for(_vtype *_vname = kboot_tag_iterate((_type), NULL); \
		_vname; \
		_vname = kboot_tag_iterate((_type), _vname))

extern bool kboot_boolean_option(const char *name);
extern uint64_t kboot_integer_option(const char *name);

#endif /* __KERNEL_KBOOT_H */
