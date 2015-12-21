/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief               Kernel limit definitions.
 */

#ifndef __KERNEL_LIMITS_H
#define __KERNEL_LIMITS_H

/** Process/threading limits. */
#define PROCESS_ID_MAX      32767       /**< Highest possible process ID. */
#define THREAD_ID_MAX       32767       /**< Highest possible thread ID. */
#define THREAD_NAME_MAX     32          /**< Maximum length of a thread name. */
#define SEMAPHORE_NAME_MAX  32          /**< Maximum length of a semaphore name. */

/** Filesystem limits. */
#define FS_PATH_MAX         4096        /**< Maximum length of a path string. */
#define FS_TYPE_MAX         32          /**< Maximum length of a filesystem type name. */
#define FS_NESTED_LINK_MAX  16          /**< Maximum number of nested symbolic links. */

/** Device manager limits. */
#define DEVICE_NAME_MAX     32          /**< Maximum length of a device name/device attribute name. */
#define DEVICE_ATTR_MAX     256         /**< Maximum length of a device attribute string value. */
#define DEVICE_PATH_MAX     256         /**< Maximum length of a device tree path. */

/** Kernel module limits. */
#define MODULE_NAME_MAX     16          /** Maximum length of a module name. */
#define MODULE_DESC_MAX     80          /** Maximum length of a module description. */

#endif /* __KERNEL_LIMITS_H */
