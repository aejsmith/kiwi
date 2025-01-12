/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Device functions.
 */

#pragma once

#include <kernel/file.h>
#include <kernel/limits.h>

__KERNEL_EXTERN_C_BEGIN

/** Start of class-specific event/request numbers. */
#define DEVICE_CLASS_EVENT_START    32
#define DEVICE_CLASS_REQUEST_START  32

/** Start of device-specific event/request numbers. */
#define DEVICE_CUSTOM_EVENT_START   1024
#define DEVICE_CUSTOM_REQUEST_START 1024

/** Standard device attribute names. */
#define DEVICE_ATTR_CLASS           "class"     /** Device class (string). */

/** Device attribute types. */
typedef enum device_attr_type {
    DEVICE_ATTR_INT8                = 0,        /**< 8-bit signed integer value. */
    DEVICE_ATTR_INT16               = 1,        /**< 16-bit signed integer value. */
    DEVICE_ATTR_INT32               = 2,        /**< 32-bit signed integer value. */
    DEVICE_ATTR_INT64               = 3,        /**< 64-bit signed integer value. */
    DEVICE_ATTR_UINT8               = 4,        /**< 8-bit unsigned integer value. */
    DEVICE_ATTR_UINT16              = 5,        /**< 16-bit unsigned integer value. */
    DEVICE_ATTR_UINT32              = 6,        /**< 32-bit unsigned integer value. */
    DEVICE_ATTR_UINT64              = 7,        /**< 64-bit unsigned integer value. */
    DEVICE_ATTR_STRING              = 8,        /**< String value. */
} device_attr_type_t;

extern status_t kern_device_open(const char *path, uint32_t access, uint32_t flags, handle_t *_handle);

// TODO: Device attribute enumeration APIs.
extern status_t kern_device_attr(handle_t handle, const char* name, device_attr_type_t type, void *buf, size_t size);

__KERNEL_EXTERN_C_END
