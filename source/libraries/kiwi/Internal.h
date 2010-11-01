/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Internal libkiwi definitions.
 */

#ifndef __INTERNAL_H
#define __INTERNAL_H

#include <kiwi/CoreDefs.h>

/** Compiler attribute/builtin macros. */
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#if CONFIG_DEBUG
extern void libkiwi_debug(const char *fmt, ...) KIWI_PUBLIC __attribute__((format(printf, 1, 2)));
#else
static inline void libkiwi_debug(const char *fmt, ...) {};
#endif
extern void libkiwi_warn(const char *fmt, ...) KIWI_PUBLIC __attribute__((format(printf, 1, 2)));
extern void libkiwi_fatal(const char *fmt, ...) KIWI_PUBLIC __attribute__((format(printf, 1, 2)));

namespace kiwi { class EventLoop; }
extern __thread kiwi::EventLoop *g_event_loop;

#endif /* __INTERNAL_H */
