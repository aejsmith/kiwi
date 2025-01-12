/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
