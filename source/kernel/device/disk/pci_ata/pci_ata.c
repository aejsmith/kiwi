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
 * @brief               ATA device library.
 */

#include <device/bus/pci.h>

#include <device/disk/ata.h>

#include <mm/malloc.h>

#include <module.h>
#include <status.h>

#define PCI_ATA_MODULE_NAME     "pci_ata"

#define PRIMARY_CHANNEL_NAME    "primary"
#define SECONDARY_CHANNEL_NAME  "secondary"

#define CMD_IO_SIZE             8
#define CTRL_IO_SIZE            1

typedef struct pci_ata_controller {
    pci_device_t *pci;
    device_t *node;
} pci_ata_controller_t;

typedef struct pci_ata_channel {
    pci_ata_controller_t *controller;
    io_region_t cmd;
    io_region_t ctrl;
} pci_ata_channel_t;

static void add_native_channel(
    pci_ata_controller_t *controller, const char *name, uint8_t cmd_bar,
    uint8_t ctrl_bar)
{
    status_t ret;

    pci_ata_channel_t *channel = kmalloc(sizeof(*channel), MM_KERNEL);

// TODO: disk_device_create, move ownership of mappings to that (and replace kfree with destroy - ownership of channel)
// use create/publish like net.

    channel->controller = controller;

    ret = device_pci_bar_map(controller->node, controller->pci, cmd_bar, MM_KERNEL, &channel->cmd);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(controller->node, LOG_ERROR, "failed to map command BAR %" PRIu8 ": %" PRId32 "\n", cmd_bar, ret);
        kfree(channel);
        return;
    }

    /* Control port is at offset 2 of the BAR. */
    ret = device_pci_bar_map_etc(
        controller->node, controller->pci, ctrl_bar, 2, CTRL_IO_SIZE, MMU_ACCESS_RW,
        MM_KERNEL, &channel->ctrl);
    {
        device_kprintf(controller->node, LOG_ERROR, "failed to map control BAR %" PRIu8 ": %" PRId32 "\n", ctrl_bar, ret);
        kfree(channel);
        return;
    }

// IRQ

    device_kprintf(controller->node, LOG_NOTICE, "  %s: native PCI (cmd: %pR, ctrl: %pR)\n", name, channel->cmd, channel->ctrl);
}

static void add_compat_channel(
    pci_ata_controller_t *controller, const char *name, pio_addr_t cmd_base,
    pio_addr_t ctrl_base, unsigned irq)
{
    pci_ata_channel_t *channel = kmalloc(sizeof(*channel), MM_KERNEL);

// TODO: disk_device_create, move ownership of mappings to that (and replace kfree with destroy - ownership of channel)
// use create/publish like net.

    channel->controller = controller;

    channel->cmd = device_pio_map(controller->node, cmd_base, CMD_IO_SIZE);
    if (channel->cmd == IO_REGION_INVALID) {
        device_kprintf(controller->node, LOG_ERROR, "failed to map command I/O @ 0x%" PRIu16 "\n", cmd_base);
        kfree(channel);
        return;
    }

    channel->ctrl = device_pio_map(controller->node, ctrl_base, CTRL_IO_SIZE);
    if (channel->ctrl == IO_REGION_INVALID) {
        device_kprintf(controller->node, LOG_ERROR, "failed to map control I/O @ 0x%" PRIu16 "\n", ctrl_base);
        kfree(channel);
        return;
    }

// IRQ

    device_kprintf(controller->node, LOG_NOTICE, "  %s: compat (cmd: %pR, ctrl: %pR)\n", name, channel->cmd, channel->ctrl);
}

static status_t pci_ata_init_device(pci_device_t *pci) {
    status_t ret;

    // TODO: Destruction: Need to free this somewhere.
    pci_ata_controller_t *controller = kmalloc(sizeof(*controller), MM_KERNEL);

    controller->pci = pci;

    ret = device_create_dir(PCI_ATA_MODULE_NAME, pci->bus.node, &controller->node);
    if (ret != STATUS_SUCCESS) {
        kfree(controller);
        return ret;
    }

    device_kprintf(controller->node, LOG_NOTICE, "found PCI ATA controller\n");

    /* Programming interface indicates which mode the channels are in:
     * Bit 0 = Primary native
     * Bit 2 = Secondary native */
    bool primary_native   = pci->prog_iface & (1 << 0);
    bool secondary_native = pci->prog_iface & (1 << 2);

    if (primary_native) {
        add_native_channel(controller, PRIMARY_CHANNEL_NAME, 0, 1);
    } else {
        /* Compatibility mode channels always have the same details. */
        add_compat_channel(controller, PRIMARY_CHANNEL_NAME, 0x1f0, 0x3f6, 14);
    }

    if (secondary_native) {
        add_native_channel(controller, SECONDARY_CHANNEL_NAME, 2, 3);
    } else {
        /* Compatibility mode channels always have the same details. */
        add_compat_channel(controller, SECONDARY_CHANNEL_NAME, 0x170, 0x376, 15);
    }

    return STATUS_SUCCESS;
}

static pci_match_t pci_ata_matches[] = {
    { PCI_MATCH_CLASS(0x01, 0x01) },
};

static pci_driver_t pci_ata_driver = {
    .matches     = PCI_MATCH_TABLE(pci_ata_matches),
    .init_device = pci_ata_init_device,
};

MODULE_NAME(PCI_ATA_MODULE_NAME);
MODULE_DESC("PCI ATA controller driver");
MODULE_DEPS(PCI_MODULE_NAME, ATA_MODULE_NAME);
MODULE_PCI_DRIVER(pci_ata_driver);
