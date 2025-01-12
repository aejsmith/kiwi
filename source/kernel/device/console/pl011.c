/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               PL011 console implementation.
 */

#include <device/console/pl011.h>

#include <device/io.h>

#include <kboot.h>
#include <kernel.h>

/** PL011 UART port definitions. */
#define PL011_REG_DR            0   /**< Data Register. */
#define PL011_REG_RSR           1   /**< Receive Status Register. */
#define PL011_REG_ECR           1   /**< Error Clear Register. */
#define PL011_REG_FR            6   /**< Flag Register. */
#define PL011_REG_IBRD          9   /**< Integer Baud Rate Register. */
#define PL011_REG_FBRD          10  /**< Fractional Baud Rate Register. */
#define PL011_REG_LCRH          11  /**< Line Control Register. */
#define PL011_REG_CR            12  /**< Control Register. */
#define PL011_REG_IFLS          13  /**< Interrupt FIFO Level Select Register. */
#define PL011_REG_IMSC          14  /**< Interrupt Mask Set/Clear Register. */
#define PL011_REG_RIS           15  /**< Raw Interrupt Status Register. */
#define PL011_REG_MIS           16  /**< Masked Interrupt Status Register. */
#define PL011_REG_ICR           17  /**< Interrupt Clear Register. */
#define PL011_REG_DMACR         18  /**< DMA Control Register. */
#define PL011_REG_COUNT         19

/** PL011 flag register bits. */
#define PL011_FR_TXFF           (1<<5)  /**< Transmit FIFO full. */
#define PL011_FR_RXFE           (1<<4)  /**< Receive FIFO empty. */

/** PL011 line control register bits. */
#define PL011_LCRH_PEN          (1<<1)  /**< Parity enable. */
#define PL011_LCRH_EPS          (1<<2)  /**< Even parity select. */
#define PL011_LCRH_STP2         (1<<3)  /**< 2 stop bits. */
#define PL011_LCRH_FEN          (1<<4)  /**< Enable FIFOs. */
#define PL011_LCRH_WLEN_SHIFT   5       /**< Shift for data bit count. */
#define PL011_LCRH_WLEN5        (0<<5)  /**< 5 data bits. */
#define PL011_LCRH_WLEN6        (1<<5)  /**< 6 data bits. */
#define PL011_LCRH_WLEN7        (2<<5)  /**< 7 data bits. */
#define PL011_LCRH_WLEN8        (3<<5)  /**< 8 data bits. */

/** PL011 control register bit definitions. */
#define PL011_CR_UARTEN         (1<<0)  /**< UART enable. */
#define PL011_CR_TXE            (1<<8)  /**< Transmit enable. */
#define PL011_CR_RXE            (1<<9)  /**< Receive enable. */

static phys_ptr_t pl011_registers_phys;
static io_region_t pl011_registers;

/** Read a UART register. */
static inline uint32_t pl011_read(unsigned reg) {
    return io_read32(pl011_registers, reg << 2);
}

/** Write a UART register. */
static inline void pl011_write(unsigned reg, uint32_t value) {
    io_write32(pl011_registers, reg << 2, value);
}

static bool pl011_serial_port_early_init(kboot_tag_serial_t *serial) {
    if (serial->type != KBOOT_SERIAL_TYPE_PL011)
        return false;

    pl011_registers      = mmio_early_map(serial->addr_virt);
    pl011_registers_phys = serial->addr;

    return true;
}

static void pl011_serial_port_init(void) {
    pl011_registers = mmio_map(pl011_registers_phys, PL011_REG_COUNT << 2, MM_BOOT);
}

static bool pl011_serial_port_rx_empty(void) {
    return pl011_read(PL011_REG_FR) & PL011_FR_RXFE;
}

static uint8_t pl011_serial_port_read(void) {
    return pl011_read(PL011_REG_DR);
}

static bool pl011_serial_port_tx_empty(void) {
    return !(pl011_read(PL011_REG_FR) & PL011_FR_TXFF);
}

static void pl011_serial_port_write(uint8_t val) {
    pl011_write(PL011_REG_DR, val);
}

const serial_port_ops_t pl011_serial_port_ops = {
    .early_init = pl011_serial_port_early_init,
    .init       = pl011_serial_port_init,
    .rx_empty   = pl011_serial_port_rx_empty,
    .read       = pl011_serial_port_read,
    .tx_empty   = pl011_serial_port_tx_empty,
    .write      = pl011_serial_port_write,
};
