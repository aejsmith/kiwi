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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

/** Maximum length of a module name. */
#define MODULE_NAME_MAX		16

/** Module information section names. */
#define MODULE_INFO_SECTION	".modinfo"
#define MODULE_EXPORT_SECTION	".modexport"

extern status_t SYSCALL(module_load)(const char *path, char *depbuf);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_MODULE_H */
