/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Scheduling definitions.
 */

#pragma once

#include <sys/types.h>

#include <time.h>

__SYS_EXTERN_C_BEGIN

extern int sched_yield(void);

__SYS_EXTERN_C_END
