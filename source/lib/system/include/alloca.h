/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Automatically freed allocation function.
 */

#pragma once

#undef alloca
#define alloca(size)    __builtin_alloca(size)
