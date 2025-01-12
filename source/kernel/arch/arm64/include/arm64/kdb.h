/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 KDB definitions.
 */

#pragma once

#include <kdb.h>

extern void arm64_kdb_brk_handler(frame_t *frame);
