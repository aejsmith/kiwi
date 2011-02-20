/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Loader type structure.
 */

#ifndef __LOADER_H
#define __LOADER_H

#include <assert.h>
#include <config.h>
#include <fs.h>
#include <ui.h>

/** Structure defining a loader type. */
typedef struct loader_type {
	/** Load the operating system.
	 * @note		Should not return.
	 * @param env		Environment for the OS. */
	void (*load)(environ_t *env) __noreturn;

	/** Display a configuration menu.
	 * @param env		Environment for the OS. */
	void (*configure)(environ_t *env);
} loader_type_t;
#if 0
/** Get the loader type from an environment.
 * @param env		Environment to get from.
 * @return		Pointer to loader type. */
static inline loader_type_t *loader_type_get(environ_t *env) {
	value_t *value;

	value = environ_lookup(env, "loader_type");
	assert(value && value->type == VALUE_TYPE_POINTER);
	return value->pointer;
}

/** Set the loader type in an environment.
 * @param env		Environment to set in.
 * @param type		Type to set. */
static inline void loader_type_set(environ_t *env, loader_type_t *type) {
	value_t value;

	value.type = VALUE_TYPE_POINTER;
	value.pointer = type;
	environ_insert(env, "loader_type", &value);
}
#endif

extern void kiwi_loader_arch_setup(environ_t *env);
extern void kiwi_loader_arch_load(fs_handle_t *handle, environ_t *env);
extern void kiwi_loader_arch_configure(environ_t *env, ui_window_t *window);
extern void kiwi_loader_arch_enter(void) __noreturn;
extern bool config_cmd_kiwi(value_list_t *args, environ_t *env);

#if CONFIG_PLATFORM_PC
extern bool config_cmd_chainload(value_list_t *args, environ_t *env);
#endif

extern void internal_error(const char *fmt, ...) __printf(1, 2) __noreturn;
extern void boot_error(const char *fmt, ...) __printf(1, 2) __noreturn;

#endif /* __LOADER_H */
