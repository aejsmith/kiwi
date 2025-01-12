/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Futex implementation.
 */

#pragma once

#include <kernel/futex.h>

struct process;

extern void futex_process_cleanup(struct process *proc);
