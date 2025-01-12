/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Path manipulation functions.
 */

#pragma once

#include <system/defs.h>

__SYS_EXTERN_C_BEGIN

extern char *core_path_basename(const char *path);
extern char *core_path_dirname(const char *path);

__SYS_EXTERN_C_END
