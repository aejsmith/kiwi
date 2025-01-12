/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Thread scheduler.
 */

#pragma once

#include <proc/thread.h>

extern void sched_reschedule(bool state);
extern void sched_post_switch(bool state);
extern void sched_preempt(void);
extern void sched_insert_thread(thread_t *thread);

extern void sched_init(void);
extern void sched_init_percpu(void);
extern void sched_enter(void) __noreturn;
