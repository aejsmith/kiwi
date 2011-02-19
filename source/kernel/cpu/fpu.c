/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		FPU context functions.
 */

#include <mm/slab.h>

#include <proc/thread.h>

#include <console.h>
#include <kernel.h>

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

		spinlock_lock(&curr_thread->lock);
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
	                                      0, MM_FATAL);
}
INITCALL(fpu_cache_init);
