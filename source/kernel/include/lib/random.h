/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Random number generation.
 */

#pragma once

#include <types.h>

extern uint64_t random_get_u64(void);
extern int64_t random_get_s64(void);
extern uint32_t random_get_u32(void);
extern int32_t random_get_s32(void);

extern void random_init(void);
