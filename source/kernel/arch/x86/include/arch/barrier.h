/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		x86 memory barrier functions.
 *
 * @todo		Use SFENCE/LFENCE/MFENCE where possible on IA32. Damn
 *			you Intel for introducing LFENCE/MFENCE in Pentium 4,
 *			but SFENCE in Pentium 3...
 *
 * @note		Critical section barriers are not required because the
 *			synchronization functions are based on atomic
 *			operations which use the LOCK prefix and LOCK forces
 *			serialization. However, we do prevent the compiler from
 *			reordering instructions.
 */

#ifndef __ARCH_BARRIER_H
#define __ARCH_BARRIER_H

/** Barrier for entering a critical section. */
#define enter_cs_barrier()	__asm__ volatile("" ::: "memory")

/** Barrier for leaving a critical section. */
#define leave_cs_barrier()	__asm__ volatile("" ::: "memory")

#ifdef __x86_64__
# define memory_barrier()	__asm__ volatile("mfence" ::: "memory")
# define read_barrier()		__asm__ volatile("lfence" ::: "memory")
# define write_barrier()	__asm__ volatile("sfence" ::: "memory")
#else
# define memory_barrier()	__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
# define read_barrier()		__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
# define write_barrier()	__asm__ volatile("lock addl $0, 0(%%esp)" ::: "memory")
#endif

#endif /* __ARCH_BARRIER_H */
