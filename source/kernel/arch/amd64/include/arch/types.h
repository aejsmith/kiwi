/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 type definitions.
 */

#pragma once

/** Format character definitions for printf(). */
#define PRIxPHYS        "lx"       /**< Format for phys_ptr_t (hexadecimal). */
#define PRIuPHYS        "lu"       /**< Format for phys_ptr_t. */

/** Integer type that can represent a pointer. */
typedef unsigned long ptr_t;

/** Integer type that can represent a physical address. */
typedef uint64_t phys_ptr_t;
typedef uint64_t phys_size_t;
