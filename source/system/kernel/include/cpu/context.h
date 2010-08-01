/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
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
