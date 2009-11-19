/* Kiwi service manager API definitions
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
 * @brief		Service manager API definitions.
 */

#ifndef __KIWI_PRIVATE_SVCMGR_H
#define __KIWI_PRIVATE_SVCMGR_H

#include <kernel/types.h>

/** Message types for the service manager. */
enum {
	/** Look up a port (input: char[], output: identifier_t). */
	SVCMGR_LOOKUP_PORT = 0,

	/** Register a port (input: svcmgr_register_port_t, output: int). */
	SVCMGR_REGISTER_PORT,
};

/** Service manager register port data structure. */
typedef struct svcmgr_register_port {
	identifier_t id;		/**< Port ID. */
	char name[];			/**< Port name. */
} svcmgr_register_port_t;

#endif /* __KIWI_PRIVATE_SVCMGR_H */
