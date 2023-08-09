/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               PCI bus interface.
 */

#pragma once

#include <kernel/device.h>

__KERNEL_EXTERN_C_BEGIN

/** PCI device class name. */
#define PCI_DEVICE_CLASS_NAME       "pci_device"

/** PCI device class attribute names. */
#define PCI_DEVICE_ATTR_VENDOR_ID   "pci_device.vendor_id"  /**< uint16 */
#define PCI_DEVICE_ATTR_DEVICE_ID   "pci_device.device_id"  /**< uint16 */
#define PCI_DEVICE_ATTR_BASE_CLASS  "pci_device.base_class" /**< uint8 */
#define PCI_DEVICE_ATTR_SUB_CLASS   "pci_device.sub_class"  /**< uint8 */

__KERNEL_EXTERN_C_END
