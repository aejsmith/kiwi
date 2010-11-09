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
 * @brief		Kernel module functions.
 */

#ifndef __KERNEL_MODULE_H
#define __KERNEL_MODULE_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of a module name/description. */
#define MODULE_NAME_MAX		16
#define MODULE_DESC_MAX		80

/** Module information section names. */
#define MODULE_INFO_SECTION	".modinfo"
#define MODULE_EXPORT_SECTION	".modexport"

/** Module information structure. */
typedef struct module_info {
	char name[MODULE_NAME_MAX];	/**< Name of the module. */
	char desc[MODULE_DESC_MAX];	/**< Description of the module. */
	size_t count;			/**< Reference count of the module. */
	size_t load_size;		/**< Size of the module in memory. */
} module_info_t;

extern status_t SYSCALL(module_load)(const char *path, char *depbuf);
extern status_t SYSCALL(module_info)(module_info_t *infop, size_t *countp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_MODULE_H */
