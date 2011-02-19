/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		CPU context functions.
 */

#ifndef __CPU_CONTEXT_H
#define __CPU_CONTEXT_H

#include <arch/context.h>
#include <types.h>

struct intr_frame;

extern void context_init(context_t *ctx, ptr_t ip, unative_t *stack);
extern void context_destroy(context_t *ctx);
extern bool context_save(context_t *ctx);
extern void context_restore(context_t *ctx) __noreturn;
extern void context_restore_frame(context_t *ctx, struct intr_frame *frame);

#endif /* __CPU_CONTEXT_H */
