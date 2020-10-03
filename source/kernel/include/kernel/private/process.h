/*
 * Copyright (C) 2009-2020 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Internal process functions/definitions.
 */

#pragma once

#include <kernel/process.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL_PRIVATE

/** Arguments passed to the userspace loader. */
typedef struct process_args {
    char *path;                     /**< Path to program. */
    char **args;                    /**< Argument array. */
    char **env;                     /**< Environment variable array. */
    size_t arg_count;               /**< Number of entries in argument array (excluding NULL). */
    size_t env_count;               /**< Number of entries in environment array (excluding NULL). */
    void *load_base;                /**< Load base of libkernel. */
} process_args_t;

/** Actions for kern_process_control(). */
#define PROCESS_LOADED          1   /**< Signal that process is loaded. */
#define PROCESS_SET_RESTORE     2   /**< Set the thread restore function. */

extern status_t kern_process_control(unsigned action, const void *in, void *out);

#ifdef __LIBKERNEL

extern status_t _kern_process_clone(handle_t *_handle);
extern status_t _kern_process_id(handle_t handle, process_id_t *_id);

#endif /* __LIBKERNEL */
#endif /* __KERNEL_PRIVATE */

#ifdef __cplusplus
}
#endif
