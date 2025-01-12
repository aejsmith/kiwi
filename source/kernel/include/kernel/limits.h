/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel limit definitions.
 */

#pragma once

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
