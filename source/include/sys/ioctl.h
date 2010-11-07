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
 * @brief		Device control function.
 */

#ifndef __SYS_IOCTL_H
#define __SYS_IOCTL_H

/** Some things that use this expect termios.h to be included by it. */
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int ioctl(int fd, int request, ...);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_IOCTL_H */
