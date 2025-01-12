/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Directory handling functions.
 */

#pragma once

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <dirent.h>

#include "libsystem.h"

/** Size of the internal directory entry buffer. */
#define DIRSTREAM_BUF_SIZE 0x1000

struct __dstream_internal {
    handle_t handle;                /**< Handle to the directory. */
    char buf[DIRSTREAM_BUF_SIZE];   /**< Buffer for entry structures. */
};
