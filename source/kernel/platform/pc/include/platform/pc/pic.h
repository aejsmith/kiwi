/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Programmable Interrupt Controller code.
 */

#ifndef __PLATFORM_PC_PIC_H
#define __PLATFORM_PC_PIC_H

/** PIC port definitions. */
#define PIC_MASTER_COMMAND	0x20	/**< Command port (Master). */
#define PIC_MASTER_DATA		0x21	/**< Data port (Master). */
#define PIC_MASTER_ELCR		0x4D0	/**< Edge/Level Control Register (Master). */
#define PIC_SLAVE_COMMAND	0xA0	/**< Command port (Slave). */
#define PIC_SLAVE_DATA		0xA1	/**< Data port (Slave). */
#define PIC_SLAVE_ELCR		0x4D1	/**< Edge/Level Control Register (Slave). */

/** PIC command definitions. */
#define PIC_COMMAND_EOI		0x20	/**< End-of-Interrupt. */

/** ICW1 (Initialisation Control Word 1) bits. */
#define PIC_ICW1_ICW4		(1<<0)	/**< Will receive ICW4 during initialisation. */
#define PIC_ICW1_SINGLE		(1<<1)	/**< If set, there is only one PIC in the system. */
#define PIC_ICW1_INIT		(1<<4)	/**< PIC is being initialised. */

/** ICW4 (Initialisation Control Word 4) bits. */
#define PIC_ICW4_8086		(1<<0)	/**< Should operate in 80x86 mode. */

extern void pic_init(void);

#endif /* __PLATFORM_PC_PIC_H */
