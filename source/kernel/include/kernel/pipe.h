/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Pipe functions.
 */

#pragma once

#include <kernel/file.h>

__KERNEL_EXTERN_C_BEGIN

extern status_t kern_pipe_create(
    uint32_t read_flags, uint32_t write_flags, handle_t *_read,
    handle_t *_write);

__KERNEL_EXTERN_C_END
