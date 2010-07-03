/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Loader type structure.
 */

#ifndef __BOOT_LOADER_H
#define __BOOT_LOADER_H

#include <boot/config.h>
#include <boot/fs.h>
#include <boot/ui.h>

#include <assert.h>

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

extern void kiwi_loader_arch_setup(environ_t *env);
extern void kiwi_loader_arch_load(fs_handle_t *handle, environ_t *env);
extern void kiwi_loader_arch_configure(environ_t *env, ui_window_t *window);
extern void kiwi_loader_arch_enter(void) __noreturn;

extern bool config_cmd_kiwi(value_list_t *args, environ_t *env);

#endif /* __BOOT_LOADER_H */
