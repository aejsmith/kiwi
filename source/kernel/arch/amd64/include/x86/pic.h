/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 Programmable Interrupt Controller code.
 */

#pragma once

#define PIC_IRQ_COUNT       16

/** PIC port definitions. */
#define PIC_MASTER_COMMAND  0x20    /**< Command port (Master). */
#define PIC_MASTER_DATA     0x21    /**< Data port (Master). */
#define PIC_MASTER_ELCR     0x4d0   /**< Edge/Level Control Register (Master). */
#define PIC_SLAVE_COMMAND   0xa0    /**< Command port (Slave). */
#define PIC_SLAVE_DATA      0xa1    /**< Data port (Slave). */
#define PIC_SLAVE_ELCR      0x4d1   /**< Edge/Level Control Register (Slave). */

/** PIC command definitions. */
#define PIC_COMMAND_EOI     0x20    /**< End-of-Interrupt. */

/** ICW1 (Initialization Control Word 1) bits. */
#define PIC_ICW1_ICW4       (1<<0)  /**< Will receive ICW4 during initialization. */
#define PIC_ICW1_SINGLE     (1<<1)  /**< If set, there is only one PIC in the system. */
#define PIC_ICW1_INIT       (1<<4)  /**< PIC is being initialized. */

/** ICW4 (Initialization Control Word 4) bits. */
#define PIC_ICW4_8086       (1<<0)  /**< Should operate in 80x86 mode. */
