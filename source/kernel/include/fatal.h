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
 * @brief		Error handling functions.
 */

#ifndef __FATAL_H
#define __FATAL_H

#include <types.h>

#ifdef LOADER
extern void fatal(const char *fmt, ...) __printf(1, 2) __noreturn;
#else

#include <lib/notifier.h>

struct intr_frame;

extern notifier_t fatal_notifier;

extern void _fatal(struct intr_frame *frame, const char *format, ...) __noreturn __printf(2, 3);

/** Print an error message and halt the kernel.
 *
 * Prints a formatted error message to the screen and breaks into KDBG. This
 * macro is a wrapper for _fatal() which passes a NULL register dump pointer.
 *
 * @param fmt		The format string for the message.
 * @param ...		The arguments to be used in the formatted message.
 */
#define fatal(fmt...)	_fatal(NULL, fmt)

#endif /* LOADER */
#endif /* __FATAL_H */
