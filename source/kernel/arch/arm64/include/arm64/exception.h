/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 exception handling.
 */

#pragma once

#include <arch/frame.h>

typedef void (*arm64_irq_handler_t)(void *private, frame_t *frame);
extern void arm64_set_irq_handler(arm64_irq_handler_t handler, void *private);

extern void arm64_irq_handler(frame_t *frame);
extern void arm64_sync_exception_handler(frame_t *frame);
extern void arm64_unhandled_exception_handler(frame_t *frame);

extern void arm64_exception_init(void);
