/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Kernel debugger.
 */

#ifndef __KDBG_H
#define __KDBG_H

#include <lib/atomic.h>
#include <lib/notifier.h>
#include <lib/string.h>

#include <kernel.h>
#include <types.h>

/** Size of KDBG input buffer. */
#define KDBG_INPUT_BUF_SIZE	1024

/** Return statuses for KDBG commands. */
#define KDBG_OK			0	/**< Command completed successfully. */
#define KDBG_FAIL		1	/**< Command failed or did not exist. */
#define KDBG_CONTINUE		2	/**< Command requires KDBG to exit. */
#define KDBG_STEP		3	/**< Command requires KDBG to single step. */

/** Entry reasons for KDBG */
#define KDBG_ENTRY_USER		0	/**< Entry by user request. */
#define KDBG_ENTRY_FATAL	1	/**< Called from fatal(). */
#define KDBG_ENTRY_BREAK	2	/**< Breakpoint. */
#define KDBG_ENTRY_WATCH	3	/**< Watchpoint. */
#define KDBG_ENTRY_STEPPED	4	/**< Stepped. */

/** Helper macro to check if a help message is wanted. */
#define KDBG_HELP(ac, av)	\
	((ac) > 1 && strcmp((av)[1], "--help") == 0)

struct intr_frame;

extern atomic_t kdbg_running;
extern struct intr_frame *curr_kdbg_frame;
extern size_t kdbg_breakpoint_id;
extern notifier_t kdbg_entry_notifier;
extern notifier_t kdbg_exit_notifier;

extern void kdbg_enter(int reason, struct intr_frame *frame);
extern unsigned char kdbg_get_char(void);
extern int kdbg_register_value(const char *name, size_t len, unative_t *regp);
extern int kdbg_cmd_backtrace(int argc, char **argv);
extern int kdbg_cmd_break(int argc, char **argv);
extern int kdbg_cmd_delete(int argc, char **argv);
extern int kdbg_cmd_list(int argc, char **argv);
extern int kdbg_cmd_regs(int argc, char **argv);
extern int kdbg_cmd_watch(int argc, char **argv);

extern int kdbg_parse_expression(char *exp, unative_t *valp, char **strp);
extern void kdbg_except_handler(unative_t num, const char *name, struct intr_frame *frame);
extern int kdbg_main(int reason, struct intr_frame *frame);

#endif /* __KDBG_H */
