/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 memory barrier functions.
 */

#pragma once

#define memory_barrier()    __asm__ volatile("mfence" ::: "memory")
#define read_barrier()      __asm__ volatile("lfence" ::: "memory")
#define write_barrier()     __asm__ volatile("sfence" ::: "memory")
