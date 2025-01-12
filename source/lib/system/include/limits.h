/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Implementation-defined constants.
 */

#pragma once

#include <kernel/limits.h>

#define PATH_MAX            FS_PATH_MAX
#define SYMLINK_MAX         FS_PATH_MAX
#define NAME_MAX            FS_PATH_MAX
#define PIPE_BUF            4096
#define FILESIZEBITS        64
#define PTHREAD_KEYS_MAX    1024

#ifndef _GCC_LIMITS_H_
#   include_next <limits.h>
#endif
