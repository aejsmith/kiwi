/* Kiwi FPU context functions
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
 * @brief		FPU context functions.
 */

#include <console/kprintf.h>

#include <mm/slab.h>

#include <proc/thread.h>

#include <init.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Cache for FPU context structures. */
static slab_cache_t *fpu_context_cache;

/** Destroy an FPU context.
 * @param ctx		Context to destroy. */
void fpu_context_destroy(fpu_context_t *ctx) {
	slab_cache_free(fpu_context_cache, ctx);
}

/** Load the current thread's FPU context. */
void fpu_request(void) {
	fpu_context_t *ctx = NULL;

	if(!curr_thread->fpu) {
		/* Safe to allocate despite being in interrupt context, as this
		 * should only be called from an interrupt in userspace. */
		ctx = slab_cache_alloc(fpu_context_cache, MM_SLEEP);

		spinlock_lock(&curr_thread->lock, 0);
		curr_thread->fpu = ctx;
		fpu_enable();
		fpu_init();
		spinlock_unlock(&curr_thread->lock);

		dprintf("fpu: created FPU context for thread %" PRId32 "(%s) (ctx: %p)\n",
		        curr_thread->id, curr_thread->name, ctx);
	} else {
		fpu_enable();
		fpu_context_restore(curr_thread->fpu);
	}
}

/** Initialise the FPU context cache. */
static void __init_text fpu_cache_init(void) {
	fpu_context_cache = slab_cache_create("fpu_context_cache", sizeof(fpu_context_t),
	                                      FPU_CONTEXT_ALIGN, NULL, NULL, NULL, NULL,
	                                      SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
INITCALL(fpu_cache_init);
