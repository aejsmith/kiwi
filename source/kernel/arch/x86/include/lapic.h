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
 * @brief		x86 local APIC definitions.
 */

#ifndef __ARCH_LAPIC_H
#define __ARCH_LAPIC_H

#include <types.h>

/** Local APIC register offsets. */
#define LAPIC_REG_APIC_ID		8	/**< Local APIC ID. */
#define LAPIC_REG_APIC_VERSION		12	/**< Local APIC Version. */
#define LAPIC_REG_TPR			32	/**< Task Priority Register (TPR). */
#define LAPIC_REG_APR			36	/**< Arbitration Priority Register (APR). */
#define LAPIC_REG_PPR			40	/**< Processor Priority Register (PPR). */
#define LAPIC_REG_EOI			44	/**< EOI. */
#define LAPIC_REG_LOGICAL_DEST		52	/**< Logical Destination. */
#define LAPIC_REG_DEST_FORMAT		56	/**< Destination Format. */
#define LAPIC_REG_SPURIOUS		60	/**< Spurious Interrupt Vector. */
#define LAPIC_REG_ISR0			64	/**< In-Service Register (ISR) - bits 0:31. */
#define LAPIC_REG_ISR1			68	/**< In-Service Register (ISR) - bits 32:63. */
#define LAPIC_REG_ISR2			72	/**< In-Service Register (ISR) - bits 64:95. */
#define LAPIC_REG_ISR3			76	/**< In-Service Register (ISR) - bits 96:127. */
#define LAPIC_REG_ISR4			80	/**< In-Service Register (ISR) - bits 128:159. */
#define LAPIC_REG_ISR5			84	/**< In-Service Register (ISR) - bits 160:191. */
#define LAPIC_REG_ISR6			88	/**< In-Service Register (ISR) - bits 192:223. */
#define LAPIC_REG_ISR7			92	/**< In-Service Register (ISR) - bits 224:255. */
#define LAPIC_REG_TMR0			96	/**< Trigger Mode Register (TMR) - bits 0:31. */
#define LAPIC_REG_TMR1			100	/**< Trigger Mode Register (TMR) - bits 32:63. */
#define LAPIC_REG_TMR2			104	/**< Trigger Mode Register (TMR) - bits 64:95. */
#define LAPIC_REG_TMR3			108	/**< Trigger Mode Register (TMR) - bits 96:127. */
#define LAPIC_REG_TMR4			112	/**< Trigger Mode Register (TMR) - bits 128:159. */
#define LAPIC_REG_TMR5			116	/**< Trigger Mode Register (TMR) - bits 160:191. */
#define LAPIC_REG_TMR6			120	/**< Trigger Mode Register (TMR) - bits 192:223. */
#define LAPIC_REG_TMR7			124	/**< Trigger Mode Register (TMR) - bits 224:255. */
#define LAPIC_REG_IRR0			128	/**< Interrupt Request Register (IRR) - bits 0:31. */
#define LAPIC_REG_IRR1			132	/**< Interrupt Request Register (IRR) - bits 32:63. */
#define LAPIC_REG_IRR2			136	/**< Interrupt Request Register (IRR) - bits 64:95. */
#define LAPIC_REG_IRR3			140	/**< Interrupt Request Register (IRR) - bits 96:127. */
#define LAPIC_REG_IRR4			144	/**< Interrupt Request Register (IRR) - bits 128:159. */
#define LAPIC_REG_IRR5			148	/**< Interrupt Request Register (IRR) - bits 160:191. */
#define LAPIC_REG_IRR6			152	/**< Interrupt Request Register (IRR) - bits 192:223. */
#define LAPIC_REG_IRR7			156	/**< Interrupt Request Register (IRR) - bits 224:255. */
#define LAPIC_REG_ERROR			160	/**< Error Status Register. */
#define LAPIC_REG_ICR0			192	/**< Interrupt Command Register (ICR) - bits 0:31. */
#define LAPIC_REG_ICR1			196	/**< Interrupt Command Register (ICR) - bits 32:63. */
#define LAPIC_REG_LVT_TIMER		200	/**< LVT Timer. */
#define LAPIC_REG_LVT_THERMAL		204	/**< LVT Thermal Sensor. */
#define LAPIC_REG_LVT_PERFMON		208	/**< LVT Performance Monitoring Counters. */
#define LAPIC_REG_LVT_LINT0		212	/**< LVT LINT0. */
#define LAPIC_REG_LVT_LINT1		216	/**< LVT LINT1. */
#define LAPIC_REG_LVT_ERROR		220	/**< LVT Error. */
#define LAPIC_REG_TIMER_INITIAL		224	/**< Timer Initial Count. */
#define LAPIC_REG_TIMER_CURRENT		228	/**< Timer Current Count. */
#define LAPIC_REG_TIMER_DIVIDER		248	/**< Timer Divide Configuration. */

/** Local APIC Divide Configuration Register values. */
#define LAPIC_TIMER_DIV1		0xB	/**< Divide by 1. */
#define LAPIC_TIMER_DIV2		0x0	/**< Divide by 2. */
#define LAPIC_TIMER_DIV4		0x1	/**< Divide by 4. */
#define LAPIC_TIMER_DIV8		0x2	/**< Divide by 8. */
#define LAPIC_TIMER_DIV16		0x3	/**< Divide by 16. */
#define LAPIC_TIMER_DIV32		0x8	/**< Divide by 32. */
#define LAPIC_TIMER_DIV64		0x9	/**< Divide by 64. */
#define LAPIC_TIMER_DIV128		0xA	/**< Divide by 128. */

/** Local APIC interrupt vectors. */
#define LAPIC_VECT_TIMER		0xf0	/**< Timer. */
#define LAPIC_VECT_SPURIOUS		0xf1	/**< Spurious. */
#define LAPIC_VECT_IPI			0xf2	/**< IPI message. */
#define LAPIC_VECT_RESCHEDULE		0xf3	/**< Reschedule. */

/** IPI delivery modes. */
#define LAPIC_IPI_FIXED			0x00	/**< Fixed (vector specified). */
#define LAPIC_IPI_NMI			0x04	/**< NMI. */
#define LAPIC_IPI_INIT			0x05	/**< INIT. */
#define LAPIC_IPI_SIPI			0x06	/**< Start-Up (SIPI). */

/** IPI destination shorthands. */
#define LAPIC_IPI_DEST_SINGLE		0x00	/**< Send to a single CPU (destination ID specified). */
#define LAPIC_IPI_DEST_ALL		0x03	/**< All, excluding self. */
#define LAPIC_IPI_DEST_ALL_INCL		0x02	/**< All, including self. */

extern bool lapic_enabled;

extern uint32_t lapic_id(void);
extern void lapic_ipi(uint8_t dest, uint8_t id, uint8_t mode, uint8_t vector);

extern bool lapic_init(void);

#endif /* __ARCH_LAPIC_H */
