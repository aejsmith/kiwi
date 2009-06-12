/* Kiwi kernel initialization code
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
 * @brief		Kernel initialization code.
 */

#ifndef __INIT_H
#define __INIT_H

#include <lib/callback.h>

extern callback_list_t init_completion_cb_list;

extern void init_bsp(void *data);
extern void init_ap(void);

#endif /* __INIT_H */
