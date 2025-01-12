/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Random number generation.
 */

#pragma once

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

extern int getentropy(void *buffer, size_t length);

__SYS_EXTERN_C_END
