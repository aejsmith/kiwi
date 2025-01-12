/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory file functions.
 */

#pragma once

#include <io/file.h>

extern object_handle_t *memory_file_create(const void *buf, size_t size);
