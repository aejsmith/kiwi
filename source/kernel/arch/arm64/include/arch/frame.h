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
 * @brief               ARM64 interrupt frame structure.
 */

#pragma once

#ifndef __ASM__

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct frame {
    unsigned long todo;
    unsigned long ip;
} __packed frame_t;

/** Return whether an interrupt frame is from user mode.
 * @param frame         Frame to check.
 * @return              Whether the frame is from user mode. */
static inline bool frame_from_user(frame_t *frame) {
    // TODO
    return false;
}

#endif /* __ASM__ */
