/* Error numbers
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
 * @brief		Error numbers.
 */

#ifndef __ERRNO_H
#define __ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

/** Standard C error number definitions. */
#define ERANGE			1	/**< Out of range. */
#define EILSEQ			2	/**< Illegal character sequence. */
#define EDOM			3	/**< Outside domain of function. */

extern int *__libsystem_errno_location(void);
#define errno (*__libsystem_errno_location())

#ifdef __cplusplus
}
#endif

#endif /* __ERRNO_H */
