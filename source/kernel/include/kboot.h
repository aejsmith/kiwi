/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               KBoot utility functions.
 */

#pragma once

#include <lib/utility.h>

#include "../../boot/include/kboot.h"

extern kboot_log_t *kboot_log;
extern size_t kboot_log_size;

extern void *kboot_tag_iterate(uint32_t type, void *current);

/** Iterate over the KBoot tag list. */
#define kboot_tag_foreach(_type, _vtype, _vname) \
    for ( \
        _vtype *_vname = kboot_tag_iterate((_type), NULL); \
        _vname; \
        _vname = kboot_tag_iterate((_type), _vname))

/** Get additional data following a KBoot tag.
 * @param tag           Tag to get data from.
 * @param offset        Offset of the data to get.
 * @return              Pointer to data. */
#define kboot_tag_data(tag, offset) \
    ((void *)(round_up((ptr_t)tag + sizeof(*tag), 8) + offset))

extern bool kboot_boolean_option(const char *name);
extern uint64_t kboot_integer_option(const char *name);
extern const char *kboot_string_option(const char *name);

extern void kboot_log_write(char ch);
extern void kboot_log_flush(void);
