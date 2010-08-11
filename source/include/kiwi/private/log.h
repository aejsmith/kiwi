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
 * @brief		Internal libkiwi logging functions.
 */

#ifndef __KIWI_PRIVATE_LOG_H
#define __KIWI_PRIVATE_LOG_H

namespace kiwi {
namespace log {

#if CONFIG_DEBUG
extern void debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
static inline void debug(const char *fmt, ...) {};
#endif
extern void warning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

}
}

#endif /* __KIWI_PRIVATE_LOG_H */
