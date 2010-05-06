/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		RTLD program interface.
 */

#include <kernel/errors.h>

#include "export.h"
#include "image.h"
#include "symbol.h"

/** Load a library into memory.
 * @param path		Path to library to open.
 * @param handlep	Where to store library handle.
 * @return		0 on success, negative error code on failure. */
static int rtld_export_library_open(const char *path, void **handle) {
	if(!path || !handle) {
		return -ERR_PARAM_INVAL;
	}
	/* FIXME: Need to call INIT's. */
	//return rtld_image_load(path, rtld_application, ELF_ET_DYN, NULL, (rtld_image_t **)handle);
	return -1;
}

/** Unload a library.
 * @param handle	Library handle. */
static void rtld_export_library_close(void *handle) {
	rtld_image_unload(handle);
}

/** Look up a symbol in a library.
 * @param handle	Library handle.
 * @param name		Name of symbol.
 * @return		Address of symbol, or NULL if not found. */
static void *rtld_export_symbol_lookup(void *handle, const char *name) {
	rtld_image_t *image = handle;
	ElfW(Addr) addr;

	if(handle && name && rtld_symbol_lookup(image, name, &addr)) {
		return (void *)addr;
	} else {
		return NULL;
	}
}

/** Table of exported functions. */
rtld_export_t rtld_exported_funcs[RTLD_EXPORT_COUNT] = {
	{ "rtld_library_open", (void *)&rtld_export_library_open },
	{ "rtld_library_close", (void *)&rtld_export_library_close },
	{ "rtld_symbol_lookup", (void *)&rtld_export_symbol_lookup },
};
