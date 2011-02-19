/*
 * Copyright (C) 2010 Alex Smith
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
