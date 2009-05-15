/* Kiwi error number definitions
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
 * @brief		Error number definitions.
 */

#ifndef __ERRORS_H
#define __ERRORS_H

#define ERR_NO_MEMORY		1	/**< No memory available. */
#define ERR_PARAM_INVAL		2	/**< Invalid parameter. */
#define ERR_WOULD_BLOCK		3	/**< Operation would block. */
#define ERR_INTERRUPTED		4	/**< Interrupted while blocking. */
#define ERR_NOT_IMPLEMENTED	5	/**< Function not implemented. */

#endif /* __ERRORS_H */
