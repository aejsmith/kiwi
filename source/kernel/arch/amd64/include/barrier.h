/* Kiwi x86 memory barrier functions
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
 * @brief		x86 memory barrier functions.
 */

#ifndef __ARCH_BARRIER_H
#define __ARCH_BARRIER_H

/* Critical section barriers are not required because the synchronization
 * functions are based on atomic operations which use the LOCK prefix and
 * LOCK forces serialization. However, we do prevent the compiler from
 * reordering instructions. */

/** Barrier for critical section entry. */
#define enter_cs_barrier()	__asm__ volatile("" ::: "memory")

/** Barrier for critical section leave. */
#define leave_cs_barrier()	__asm__ volatile("" ::: "memory")

/* TODO: Use MFENCE and friends where possible for IA32. */
#if CONFIG_ARCH_64BIT
# define memory_barrier()	__asm__ volatile("mfence" ::: "memory")
# define read_barrier()		__asm__ volatile("lfence" ::: "memory")
# define write_barrier()	__asm__ volatile("sfence" ::: "memory")
#else
# define memory_barrier()	__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
# define read_barrier()		__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
# define write_barrier()	__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
#endif

#endif /* __ARCH_BARRIER_H */
