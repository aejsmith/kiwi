/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel module functions.
 */

#pragma once

#include <kernel/limits.h>
#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

/** Module information structure. */
typedef struct module_info {
    char name[MODULE_NAME_MAX];     /**< Name of the module. */
    char desc[MODULE_DESC_MAX];     /**< Description of the module. */
    size_t count;                   /**< Reference count of the module. */
    size_t load_size;               /**< Size of the module in memory. */
} module_info_t;

extern status_t kern_module_load(const char *path, char *depbuf);
extern status_t kern_module_info(module_info_t *_info, size_t *_count);

__KERNEL_EXTERN_C_END
