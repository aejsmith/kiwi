/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               PCI bus manager.
 */

#include <device/bus/pci.h>

#include <module.h>
#include <status.h>

/** PCI device bus. */
__export bus_t pci_bus;

/** Match a PCI device to a driver. */
static bool pci_bus_match_device(device_t *device, bus_driver_t *driver) {
    return false;
}

/** Initialize a PCI device. */
static status_t pci_bus_init_device(device_t *device, bus_driver_t *driver) {
    return STATUS_NOT_IMPLEMENTED;
}

static bus_type_t pci_bus_type = {
    .name         = "pci",
    .device_class = PCI_DEVICE_CLASS_NAME,
    .match_device = pci_bus_match_device,
    .init_device  = pci_bus_init_device,
};

static status_t pci_init(void) {
    return bus_init(&pci_bus, &pci_bus_type);
}

static status_t pci_unload(void) {
    return bus_destroy(&pci_bus);
}

MODULE_NAME(PCI_MODULE_NAME);
MODULE_DESC("PCI bus manager");
MODULE_FUNCS(pci_init, pci_unload);
