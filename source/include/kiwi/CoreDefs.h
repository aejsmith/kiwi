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
 * @brief		Core Kiwi API definitions.
 */

#ifndef __KIWI_COREDEFS_H
#define __KIWI_COREDEFS_H

#include <kernel/status.h>
#include <kernel/types.h>

/** Visibility attribute definitions. */
#define KIWI_PUBLIC		__attribute__((visibility("default")))
#define KIWI_PRIVATE		__attribute__((visibility("hidden")))

/** Definitions for the Kiwi namespace. */
#define KIWI_BEGIN_NAMESPACE	namespace kiwi {
#define KIWI_END_NAMESPACE	}
#define KIWI_BEGIN_INTERNAL	namespace internal {
#define KIWI_END_INTERNAL	}

#endif /* __KIWI_COREDEFS_H */
