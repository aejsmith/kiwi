/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               NS16550 console implementation.
 */

#include <device/console/ns16550.h>

#include <device/io.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>

/** UART port definitions. */
#define NS16550_REG_RHR         0       /**< Receive Holding Register (R). */
#define NS16550_REG_THR         0       /**< Transmit Holding Register (W). */
#define NS16550_REG_DLL         0       /**< Divisor Latches Low (R/W). */
#define NS16550_REG_DLH         1       /**< Divisor Latches High (R/W). */
#define NS16550_REG_IER         1       /**< Interrupt Enable Register (R/W). */
#define NS16550_REG_IIR         2       /**< Interrupt Identification Register (R). */
#define NS16550_REG_FCR         2       /**< FIFO Control Register (W). */
#define NS16550_REG_LCR         3       /**< Line Control Register (R/W). */
#define NS16550_REG_MCR         4       /**< Modem Control Register (R/W). */
#define NS16550_REG_LSR         5       /**< Line Status Register (R). */
#define NS16550_REG_COUNT       6

/** FIFO Control Register (FCR) bits. */
#define NS16550_FCR_FIFO_EN     (1<<0)  /**< FIFO enable. */
#define NS16550_FCR_CLEAR_RX    (1<<1)  /**< Receiver soft reset. */
#define NS16550_FCR_CLEAR_TX    (1<<2)  /**< Transmitter soft reset. */
#define NS16550_FCR_DMA_EN      (1<<3)  /**< DMA enable. */

/** Line Control Register (LCR) bits. */
#define NS16550_LCR_WLS_MASK    0x03    /**< Word length select mask. */
#define NS16550_LCR_WLS_5       0x00    /**< 5 bit character length. */
#define NS16550_LCR_WLS_6       0x01    /**< 6 bit character length. */
#define NS16550_LCR_WLS_7       0x02    /**< 7 bit character length. */
#define NS16550_LCR_WLS_8       0x03    /**< 8 bit character length. */
#define NS16550_LCR_STOP        (1<<2)  /**< Stop bit length select. */
#define NS16550_LCR_PARITY      (1<<3)  /**< Parity enable. */
#define NS16550_LCR_EPAR        (1<<4)  /**< Even parity. */
#define NS16550_LCR_SPAR        (1<<5)  /**< Sticky parity. */
#define NS16550_LCR_SBRK        (1<<6)  /**< Set break. */
#define NS16550_LCR_DLAB        (1<<7)  /**< Divisor Latch Access Bit. */

/** Modem Control Register (MCR) bits. */
#define NS16550_MCR_DTR         (1<<0)  /**< DTR. */
#define NS16550_MCR_RTS         (1<<1)  /**< RTS. */

/** Line Status Register (LSR) bits. */
#define NS16550_LSR_DR          (1<<0)  /**< Data ready. */
#define NS16550_LSR_OE          (1<<1)  /**< Overrun. */
#define NS16550_LSR_PE          (1<<2)  /**< Parity error. */
#define NS16550_LSR_FE          (1<<3)  /**< Framing error. */
#define NS16550_LSR_BI          (1<<4)  /**< Break. */
#define NS16550_LSR_THRE        (1<<5)  /**< THR empty. */
#define NS16550_LSR_TEMT        (1<<6)  /**< Transmitter empty. */
#define NS16550_LSR_ERR         (1<<7)  /**< Error. */

static phys_ptr_t ns16550_registers_phys;
static io_region_t ns16550_registers = IO_REGION_INVALID;
static unsigned ns16550_registers_shift;

/** Read a UART register. */
static inline uint32_t ns16550_read(unsigned reg) {
    return io_read8(ns16550_registers, reg << ns16550_registers_shift);
}

/** Write a UART register. */
static inline void ns16550_write(unsigned reg, uint32_t value) {
    io_write8(ns16550_registers, reg << ns16550_registers_shift, value);
}

static bool ns16550_serial_port_early_init(kboot_tag_serial_t *serial) {
    switch (serial->type) {
        case KBOOT_SERIAL_TYPE_NS16550:
        case KBOOT_SERIAL_TYPE_BCM2835_AUX:
            break;
        default:
            return false;
    }

    switch (serial->io_type) {
        case KBOOT_IO_TYPE_MMIO:
            ns16550_registers       = mmio_early_map(serial->addr_virt);
            ns16550_registers_phys  = serial->addr;
            ns16550_registers_shift = 2;
            break;
        #if ARCH_HAS_PIO
        case KBOOT_IO_TYPE_PIO:
            ns16550_registers       = pio_map(serial->addr, NS16550_REG_COUNT);
            ns16550_registers_shift = 0;
            break;
        #endif
        default:
            return false;
    }

    /* See if this looks like a 16550. Check for registers that are known 0. */
    if (ns16550_read(NS16550_REG_IIR) & 0x30 || ns16550_read(NS16550_REG_MCR) & 0xe0) {
        ns16550_registers      = IO_REGION_INVALID;
        ns16550_registers_phys = 0;
        return false;
    }

    return true;
}

static void ns16550_serial_port_init(void) {
    if (ns16550_registers_phys != 0) {
        ns16550_registers = mmio_map(
            ns16550_registers_phys, NS16550_REG_COUNT << ns16550_registers_shift, MM_BOOT);
    }
}

static bool ns16550_serial_port_rx_empty(void) {
    return !(ns16550_read(NS16550_REG_LSR) & NS16550_LSR_DR);
}

static uint8_t ns16550_serial_port_read(void) {
    return ns16550_read(NS16550_REG_RHR);
}

static bool ns16550_serial_port_tx_empty(void) {
    return ns16550_read(NS16550_REG_LSR) & NS16550_LSR_THRE;
}

static void ns16550_serial_port_write(uint8_t val) {
    ns16550_write(NS16550_REG_THR, val);
}

const serial_port_ops_t ns16550_serial_port_ops = {
    .early_init = ns16550_serial_port_early_init,
    .init       = ns16550_serial_port_init,
    .rx_empty   = ns16550_serial_port_rx_empty,
    .read       = ns16550_serial_port_read,
    .tx_empty   = ns16550_serial_port_tx_empty,
    .write      = ns16550_serial_port_write,
};

void ns16550_serial_configure(kboot_tag_serial_t *serial, uint32_t clock_rate) {
    if (ns16550_registers == IO_REGION_INVALID)
        return;

    assert(clock_rate != 0);

    /* Disable all interrupts, disable the UART while configuring. */
    ns16550_write(NS16550_REG_IER, 0);
    ns16550_write(NS16550_REG_FCR, 0);

    /* Set DLAB to enable access to divisor registers. */
    ns16550_write(NS16550_REG_LCR, NS16550_LCR_DLAB);

    /* Program the divisor to set the baud rate. */
    uint16_t divisor = (clock_rate / 16) / serial->baud_rate;
    ns16550_write(NS16550_REG_DLL, divisor & 0xff);
    ns16550_write(NS16550_REG_DLH, (divisor >> 8) & 0x3f);

    /* Determine the LCR value. */
    uint8_t lcr = NS16550_LCR_WLS_5 + serial->data_bits - 5;
    if (serial->stop_bits == 2)
        lcr |= NS16550_LCR_STOP;
    if (serial->parity != KBOOT_SERIAL_PARITY_NONE) {
        lcr |= NS16550_LCR_PARITY;
        if (serial->parity == KBOOT_SERIAL_PARITY_EVEN)
            lcr |= NS16550_LCR_EPAR;
    }

    /* Switch to operational mode. */
    ns16550_write(NS16550_REG_LCR, lcr);

    /* Clear and enable FIFOs. */
    ns16550_write(NS16550_REG_FCR, NS16550_FCR_FIFO_EN | NS16550_FCR_CLEAR_RX | NS16550_FCR_CLEAR_TX);

    /* Enable RTS/DTR. */
    ns16550_write(NS16550_REG_MCR, NS16550_MCR_DTR | NS16550_MCR_RTS);
}
