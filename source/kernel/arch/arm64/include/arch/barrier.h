/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 memory barrier functions.
 */

#pragma once

#define memory_barrier()    __asm__ volatile("dsb sy" ::: "memory")
#define read_barrier()      __asm__ volatile("dsb ld" ::: "memory")
#define write_barrier()     __asm__ volatile("dsb st" ::: "memory")
