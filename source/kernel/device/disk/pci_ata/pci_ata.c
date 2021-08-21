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
 * @brief               PCI ATA controller driver.
 *
 * References:
 * - PCI IDE Controller Specification
 *   http://www.bswd.com/pciide.pdf
 * - Programming Interface for Bus Master IDE Controller
 *   http://www.bswd.com/idems100.pdf
 */

#include <device/bus/pci.h>

#include <device/disk/ata.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>

#define PCI_ATA_MODULE_NAME         "pci_ata"

#define PRIMARY_CHANNEL_NAME        "primary"
#define SECONDARY_CHANNEL_NAME      "secondary"

#define CMD_IO_SIZE                 0x8
#define CTRL_IO_SIZE                0x1
#define BUS_MASTER_IO_SIZE          0x10

/** Bus master register definitions (offset by 0x8 for secondary). */
#define PCI_ATA_BM_REG_CMD          0x0
#define PCI_ATA_BM_REG_STATUS       0x2
#define PCI_ATA_BM_REG_PRDT_ADDRESS 0x4

/** Bus master command register bit definitions. */
#define PCI_ATA_BM_CMD_RWC          (1<<3)  /**< Direction (1 = read from device). */
#define PCI_ATA_BM_CMD_START        (1<<0)  /**< Start/Stop Bus Master. */

/** Bus master status register bit definitions. */
#define PCI_ATA_BM_STATUS_ACTIVE    (1<<0)  /**< Bus Master IDE Active. */
#define PCI_ATA_BM_STATUS_ERROR     (1<<1)  /**< Error. */
#define PCI_ATA_BM_STATUS_INTERRUPT (1<<2)  /**< Interrupt. */
#define PCI_ATA_BM_STATUS_CAPABLE0  (1<<5)  /**< Drive 0 DMA Capable. */
#define PCI_ATA_BM_STATUS_CAPABLE1  (1<<6)  /**< Drive 1 DMA Capable. */
#define PCI_ATA_BM_STATUS_SIMPLEX   (1<<7)  /**< Simplex only. */

typedef struct pci_ata_controller {
    device_t *node;
    pci_device_t *pci;
} pci_ata_controller_t;

typedef struct pci_ata_channel {
    ata_sff_channel_t sff;

    pci_ata_controller_t *controller;
    io_region_t cmd;
    io_region_t ctrl;
    io_region_t bus_master;
} pci_ata_channel_t;

DEFINE_CLASS_CAST(pci_ata_channel, ata_sff_channel, sff);

static uint8_t pci_ata_channel_read_ctrl(ata_sff_channel_t *_channel, uint8_t reg) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);
    return io_read8(channel->ctrl, reg);
}

static void pci_ata_channel_write_ctrl(ata_sff_channel_t *_channel, uint8_t reg, uint8_t val) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);
    io_write8(channel->ctrl, reg, val);
}

static uint8_t pci_ata_channel_read_cmd(ata_sff_channel_t *_channel, uint8_t reg) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);
    return io_read8(channel->cmd, reg);
}

static void pci_ata_channel_write_cmd(ata_sff_channel_t *_channel, uint8_t reg, uint8_t val) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);
    io_write8(channel->cmd, reg, val);
}

static void pci_ata_channel_read_pio(ata_sff_channel_t *_channel, void *buf, size_t count) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);

    assert(!(count % 2));

    io_read16s(channel->cmd, ATA_CMD_REG_DATA, (count / 2), (uint16_t *)buf);
}

static void pci_ata_channel_write_pio(ata_sff_channel_t *_channel, const void *buf, size_t count) {
    pci_ata_channel_t *channel = cast_pci_ata_channel(_channel);

    assert(!(count % 2));

    io_write16s(channel->cmd, ATA_CMD_REG_DATA, (count / 2), (const uint16_t *)buf);
}

static ata_sff_channel_ops_t pci_ata_channel_ops = {
    .read_ctrl  = pci_ata_channel_read_ctrl,
    .write_ctrl = pci_ata_channel_write_ctrl,
    .read_cmd   = pci_ata_channel_read_cmd,
    .write_cmd  = pci_ata_channel_write_cmd,
    .read_pio   = pci_ata_channel_read_pio,
    .write_pio  = pci_ata_channel_write_pio,
};

static irq_status_t pci_ata_early_irq(unsigned num, void *_channel) {
    pci_ata_channel_t *channel = _channel;

    /* Check whether this device has raised an interrupt. */
    uint8_t status = io_read8(channel->bus_master, PCI_ATA_BM_REG_STATUS);
    if (!(status & PCI_ATA_BM_STATUS_INTERRUPT))
        return IRQ_UNHANDLED;

    /* Clear interrupt flag. The low 3 bits are write 1 to clear, so don't clear
     * error/active. */
    status = (status & 0xf8) | PCI_ATA_BM_STATUS_INTERRUPT;
    io_write8(channel->bus_master, PCI_ATA_BM_REG_STATUS, status);

    /* Clear INTRQ. */
    io_read8(channel->cmd, ATA_CMD_REG_STATUS);

    return IRQ_RUN_THREAD;
}

static void pci_ata_irq(unsigned num, void *_channel) {
    pci_ata_channel_t *channel = _channel;

    ata_channel_interrupt(&channel->sff.ata);
}

static void add_channel(pci_ata_channel_t *channel, const char *mode) {
    /* Check channel presence by writing a value to the low LBA port on the
     * channel, then reading it back. If the value is the same, it is present. */
    io_write8(channel->cmd, ATA_CMD_REG_LBA_LOW, 0xab);
    if (io_read8(channel->cmd, ATA_CMD_REG_LBA_LOW) != 0xab) {
        ata_channel_destroy(&channel->sff.ata);
        return;
    }

    pci_enable_master(channel->controller->pci, true);

    channel->sff.ata.caps = ATA_CHANNEL_CAP_PIO | ATA_CHANNEL_CAP_DMA | ATA_CHANNEL_CAP_SLAVE;
    channel->sff.ops      = &pci_ata_channel_ops;

    device_kprintf(
        channel->sff.ata.node, LOG_NOTICE, "%s mode (cmd: %pR, ctrl: %pR, bus_master: %pR)\n",
        mode, channel->cmd, channel->ctrl, channel->bus_master);

    status_t ret = ata_channel_publish(&channel->sff.ata);
    if (ret != STATUS_SUCCESS)
        ata_channel_destroy(&channel->sff.ata);
}

static void add_native_channel(
    pci_ata_controller_t *controller, const char *name, uint8_t cmd_bar,
    uint8_t ctrl_bar, io_region_t bus_master)
{
    status_t ret;

    pci_ata_channel_t *channel = kmalloc(sizeof(*channel), MM_KERNEL);

    ret = ata_sff_channel_create(&channel->sff, name, controller->node);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(controller->node, LOG_ERROR, "failed to create channel: %d\n", ret);
        kfree(channel);
        return;
    }

    device_t *node = channel->sff.ata.node;

    device_add_kalloc(node, channel);

    channel->controller = controller;
    channel->bus_master = bus_master;

    ret = device_pci_bar_map(node, controller->pci, cmd_bar, MM_KERNEL, &channel->cmd);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(node, LOG_ERROR, "failed to map command BAR %" PRIu8 ": %" PRId32 "\n", cmd_bar, ret);
        goto err_destroy;
    }

    /* Control port is at offset 2 of the BAR. */
    ret = device_pci_bar_map_etc(
        node, controller->pci, ctrl_bar, 2, CTRL_IO_SIZE, MMU_ACCESS_RW,
        MM_KERNEL, &channel->ctrl);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(node, LOG_ERROR, "failed to map control BAR %" PRIu8 ": %" PRId32 "\n", ctrl_bar, ret);
        goto err_destroy;
    }

    ret = device_pci_irq_register(node, controller->pci, pci_ata_early_irq, pci_ata_irq, channel);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(node, LOG_ERROR, "failed to register IRQ: %" PRId32 "\n", ret);
        goto err_destroy;
    }

    add_channel(channel, "native PCI");
    return;

err_destroy:
    ata_channel_destroy(&channel->sff.ata);
    return;
}

static void add_compat_channel(
    pci_ata_controller_t *controller, const char *name, pio_addr_t cmd_base,
    pio_addr_t ctrl_base, io_region_t bus_master, unsigned irq)
{
    status_t ret;

    pci_ata_channel_t *channel = kmalloc(sizeof(*channel), MM_KERNEL);

    ret = ata_sff_channel_create(&channel->sff, name, controller->node);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(controller->node, LOG_ERROR, "failed to create channel: %d\n", ret);
        kfree(channel);
        return;
    }

    device_t *node = channel->sff.ata.node;

    device_add_kalloc(node, channel);

    channel->controller = controller;
    channel->bus_master = bus_master;

    channel->cmd = device_pio_map(node, cmd_base, CMD_IO_SIZE);
    if (channel->cmd == IO_REGION_INVALID) {
        device_kprintf(node, LOG_ERROR, "failed to map command I/O @ 0x%" PRIu16 "\n", cmd_base);
        goto err_destroy;
    }

    channel->ctrl = device_pio_map(node, ctrl_base, CTRL_IO_SIZE);
    if (channel->ctrl == IO_REGION_INVALID) {
        device_kprintf(node, LOG_ERROR, "failed to map control I/O @ 0x%" PRIu16 "\n", ctrl_base);
        goto err_destroy;
    }

    ret = device_irq_register(node, irq, pci_ata_early_irq, pci_ata_irq, channel);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(node, LOG_ERROR, "failed to register IRQ: %" PRId32 "\n", ret);
        goto err_destroy;
    }

    add_channel(channel, "compatibility");
    return;

err_destroy:
    ata_channel_destroy(&channel->sff.ata);
    return;
}

static status_t pci_ata_init_device(pci_device_t *pci) {
    status_t ret;

    pci_ata_controller_t *controller = kmalloc(sizeof(*controller), MM_KERNEL);

    controller->pci = pci;

    device_attr_t attrs[] = {
        { DEVICE_ATTR_CLASS, DEVICE_ATTR_STRING, { .string = "pci_ata_controller" } },
    };

    ret = device_create(
        PCI_ATA_MODULE_NAME, pci->bus.node, NULL, controller, attrs,
        array_size(attrs), &controller->node);
    if (ret != STATUS_SUCCESS) {
        kfree(controller);
        return ret;
    }

    device_add_kalloc(controller->node, controller);

    device_kprintf(controller->node, LOG_NOTICE, "found PCI ATA controller\n");

    io_region_t bus_master;
    ret = device_pci_bar_map_etc(
        controller->node, controller->pci, 4, 0, BUS_MASTER_IO_SIZE,
        MMU_ACCESS_RW, MM_KERNEL, &bus_master);
    if (ret != STATUS_SUCCESS) {
        device_kprintf(controller->node, LOG_ERROR, "failed to map bus master BAR: %" PRId32 "\n", ret);
        kfree(controller);
        return ret;
    }

    /* Programming interface indicates which mode the channels are in:
     * Bit 0 = Primary native
     * Bit 2 = Secondary native */
    bool primary_native   = pci->prog_iface & (1 << 0);
    bool secondary_native = pci->prog_iface & (1 << 2);

    if (primary_native) {
        add_native_channel(controller, PRIMARY_CHANNEL_NAME, 0, 1, bus_master);
    } else {
        /* Compatibility mode channels always have the same details. */
        add_compat_channel(controller, PRIMARY_CHANNEL_NAME, 0x1f0, 0x3f6, bus_master, 14);
    }

    if (secondary_native) {
        add_native_channel(controller, SECONDARY_CHANNEL_NAME, 2, 3, bus_master + 0x8);
    } else {
        /* Compatibility mode channels always have the same details. */
        add_compat_channel(controller, SECONDARY_CHANNEL_NAME, 0x170, 0x376, bus_master + 0x8, 15);
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
