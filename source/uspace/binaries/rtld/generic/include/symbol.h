/* Kiwi RTLD symbol functions
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
 * @brief		RTLD symbol functions.
 */

#ifndef __RTLD_SYMBOL_H
#define __RTLD_SYMBOL_H

#include <rtld/image.h>
#include <stdbool.h>

extern bool rtld_symbol_lookup(rtld_image_t *start, const char *name, ElfW(Addr) *addrp);

#endif /* __RTLD_SYMBOL_H */
