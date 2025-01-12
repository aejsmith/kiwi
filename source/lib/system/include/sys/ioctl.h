/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device control function.
 */

#pragma once

/** Some things that use this expect termios.h to be included by it. */
#include <termios.h>

__SYS_EXTERN_C_BEGIN

extern int ioctl(int fd, int request, ...);

__SYS_EXTERN_C_END
