/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Thread scheduler.
 */

#ifndef __PROC_SCHED_H
#define __PROC_SCHED_H

extern void sched_yield(void);
extern void sched_preempt(void);
extern void sched_preempt_disable(void);
extern void sched_preempt_enable(void);

extern void sched_init(void);
extern void sched_enter(void);

#endif /* __PROC_SCHED_H */
