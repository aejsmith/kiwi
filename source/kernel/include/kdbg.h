/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Kernel debugger.
 */

#ifndef __KDBG_H
#define __KDBG_H

#include <lib/atomic.h>
#include <lib/notifier.h>
#include <lib/string.h>

#include <console.h>
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
#define KDBG_ENTRY_STEPPED	3	/**< Stepped one instruction. */

/** Helper macro to check if a help message is wanted. */
#define KDBG_HELP(ac, av)	\
	((ac) > 1 && strcmp((av)[1], "--help") == 0)

struct intr_frame;

extern atomic_t kdbg_running;
extern struct intr_frame *curr_kdbg_frame;
extern notifier_t kdbg_entry_notifier;
extern notifier_t kdbg_exit_notifier;

/** Functions implemented by the architecture/platform. */
extern void kdbg_enter(int reason, struct intr_frame *frame);
extern unsigned char kdbg_get_char(void);
extern int kdbg_register_value(const char *name, size_t len, unative_t *regp);
extern int kdbg_cmd_backtrace(int argc, char **argv);
extern int kdbg_cmd_bdelete(int argc, char **argv);
extern int kdbg_cmd_bdisable(int argc, char **argv);
extern int kdbg_cmd_benable(int argc, char **argv);
extern int kdbg_cmd_break(int argc, char **argv);
extern int kdbg_cmd_regs(int argc, char **argv);

extern int kdbg_parse_expression(char *exp, unative_t *valp, char **strp);
extern void kdbg_except_handler(unative_t num, const char *name, struct intr_frame *frame);
extern int kdbg_main(int reason, struct intr_frame *frame);

#endif /* __KDBG_H */
