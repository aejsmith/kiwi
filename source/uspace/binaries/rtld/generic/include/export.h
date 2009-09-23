/* Kiwi RTLD program interface
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
 * @brief		RTLD program interface.
 */

#ifndef __RTLD_EXPORT_H
#define __RTLD_EXPORT_H

/** Exported function structure. */
typedef struct rtld_export {
	const char *name;		/**< Exported symbol name. */
	void *addr;			/**< Address to map to. */
} rtld_export_t;

/** Number of exported functions. */
#define RTLD_EXPORT_COUNT	3

extern rtld_export_t rtld_exported_funcs[RTLD_EXPORT_COUNT];

#endif /* __RTLD_EXPORT_H */
