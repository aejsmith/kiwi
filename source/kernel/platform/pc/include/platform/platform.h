/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		PC platform header.
 */

#ifndef __PLATFORM_PLATFORM_H
#define __PLATFORM_PLATFORM_H

#include <kargs.h>

extern void platform_postmm_init(kernel_args_t *args);

/** Not required on this platform. */
#define platform_premm_init(args)
#define platform_ap_init(args)

#endif /* __PLATFORM_PLATFORM_H */
