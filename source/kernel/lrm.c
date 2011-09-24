/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Low Resource Manager.
 *
 * The Low Resource Manager is a centralised system for reclaiming resources
 * when the levels of free resources are running low. It allows kernel
 * subsystems (such as the slab allocator and the page cache) to register
 * handler functions to reclaim resources.
 */

#include <lib/utility.h>

#include <mm/heap.h>
#include <mm/page.h>

#include <proc/thread.h>

#include <sync/condvar.h>
#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <kdbg.h>
#include <kernel.h>
#include <lrm.h>
#include <status.h>
#include <symbol.h>

#if CONFIG_LRM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Interval for low resource checks. */
#define LRM_INTERVAL		SECS2USECS(5)

/** Resource limits (all in MB).
 * @todo		Investigate better values for these. I just made them
 *			up. */
#define MEMORY_ADVISORY_LIMIT	32
#define MEMORY_LOW_LIMIT	16
#define MEMORY_CRITICAL_LIMIT	8
#define KASPACE_ADVISORY_LIMIT	64
#define KASPACE_LOW_LIMIT	32
#define KASPACE_CRITICAL_LIMIT	16

/** List of registered handlers, ordered with highest priority first. */
static LIST_DECLARE(lrm_handlers);
static SPINLOCK_DECLARE(lrm_handlers_lock);

/** LRM thread. */
static thread_t *lrm_thread = NULL;
static CONDVAR_DECLARE(lrm_request_cvar);
static CONDVAR_DECLARE(lrm_response_cvar);
static MUTEX_DECLARE(lrm_response_lock, 0);

/** Main function for the LRM thread.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void lrm_thread_func(void *arg1, void *arg2) {
	lrm_handler_t *handler;
	int level;

	while(true) {
		/* Wait either for the check interval, or until a call to
		 * lrm_reclaim() requests that we run. */
		condvar_wait_etc(&lrm_request_cvar, NULL, NULL, LRM_INTERVAL, 0);

		/* Invoke handlers that can reclaim any resource types that are
		 * not currently at an OK level. TODO: Should only move onto
		 * the next handler once the previous cannot do anything. */
		LIST_FOREACH(&lrm_handlers, iter) {
			handler = list_entry(iter, lrm_handler_t, header);

			level = lrm_level(handler->types);
			if(level != RESOURCE_LEVEL_OK) {
				handler->func(level);
			}
		}

		/* Wake up anyone waiting for us to finish. */
		mutex_lock(&lrm_response_lock);
		condvar_broadcast(&lrm_response_cvar);
		mutex_unlock(&lrm_response_lock);
	}
}

/** Register a low resource handler.
 * @param handler	Handler to register. */
void lrm_handler_register(lrm_handler_t *handler) {
	lrm_handler_t *exist;

	list_init(&handler->header);

	/* Add the handler to the list, keeping it ordered with highest
	 * priority first. */
	spinlock_lock(&lrm_handlers_lock);
	LIST_FOREACH(&lrm_handlers, iter) {
		exist = list_entry(iter, lrm_handler_t, header);
		if(handler->priority < exist->priority) {
			list_add_before(&exist->header, &handler->header);
		}
	}
	if(list_empty(&handler->header)) {
		list_append(&lrm_handlers, &handler->header);
	}
	spinlock_unlock(&lrm_handlers_lock);
}

/** Unregister a low resource handler.
 * @param handler	Handler to unregister. */
void lrm_handler_unregister(lrm_handler_t *handler) {
	spinlock_lock(&lrm_handlers_lock);
	list_remove(&handler->header);
	spinlock_unlock(&lrm_handlers_lock);
}

/** Get the free memory level.
 * @return		Free physical memory level. */
static int compute_memory_level(void) {
	page_stats_t stats;
	uint64_t free;

	page_stats_get(&stats);
	free = ROUND_UP(stats.free, 1024 * 1024) / 1024 / 1024;
	if(free <= MEMORY_CRITICAL_LIMIT) {
		return RESOURCE_LEVEL_CRITICAL;
	} else if(free <= MEMORY_LOW_LIMIT) {
		return RESOURCE_LEVEL_LOW;
	} else if(free <= MEMORY_ADVISORY_LIMIT) {
		return RESOURCE_LEVEL_ADVISORY;
	} else {
		return RESOURCE_LEVEL_OK;
	}
}

/** Get the free kernel address space level.
 * @return		Free kernel address space level. */
static int compute_kaspace_level(void) {
	// FIXME
#if 0
	uint64_t free;

	free = ROUND_UP(kheap_raw_arena.total_size - kheap_raw_arena.used_size, 1024 * 1024);
	free = free / 1024 / 1024;
	if(free <= KASPACE_CRITICAL_LIMIT) {
		return RESOURCE_LEVEL_CRITICAL;
	} else if(free <= KASPACE_LOW_LIMIT) {
		return RESOURCE_LEVEL_LOW;
	} else if(free <= KASPACE_ADVISORY_LIMIT) {
		return RESOURCE_LEVEL_ADVISORY;
	} else {
#endif
		return RESOURCE_LEVEL_OK;
	//}
}

/** Get the level of available resources.
 * @param types		Types to get level of.
 * @return		Level of resource available. This will be the lowest
 *			level out of the levels for all the types specified. */
int lrm_level(uint32_t types) {
	uint64_t level = RESOURCE_LEVEL_OK, ret;

	if(types & RESOURCE_TYPE_MEMORY) {
		ret = compute_memory_level();
		level = MAX(ret, level);
	}
	if(types & RESOURCE_TYPE_KASPACE) {
		ret = compute_kaspace_level();
		level = MAX(ret, level);
	}
	return level;
}

/** Attempt to reclaim a resource.
 * @param type		Type of resource to reclaim. */
void lrm_reclaim(uint32_t type) {
	/* If the LRM has not been initialised yet, then we can't do anything. */
	if(unlikely(!lrm_thread)) {
		switch(type) {
		case RESOURCE_TYPE_MEMORY:
			fatal("You do not have enough memory to run Kiwi");
			break;
		case RESOURCE_TYPE_KASPACE:
			fatal("Exhausted kernel address space during early boot");
			break;
		}
	}

	/* If we are the LRM thread, we're fucked. */
	if(curr_thread == lrm_thread) {
		fatal("Out of memory during reclaim");
	}

	/* Wake the thread and wait for it to finish. */
	mutex_lock(&lrm_response_lock);
	condvar_broadcast(&lrm_request_cvar);
	condvar_wait(&lrm_response_cvar, &lrm_response_lock, NULL);
	mutex_unlock(&lrm_response_lock);
}

/** Print a resource state.
 * @param type		Type of resource.
 * @param name		Name of resource. */
static void print_resource_state(uint32_t type, const char *name) {
	int level = lrm_level(type);
	kprintf(LOG_NONE, "%-21s ", name);
	switch(level) {
	case RESOURCE_LEVEL_OK:
		kprintf(LOG_NONE, "OK\n");
		return;
	case RESOURCE_LEVEL_ADVISORY:
		kprintf(LOG_NONE, "Advisory\n");
		return;
	case RESOURCE_LEVEL_LOW:
		kprintf(LOG_NONE, "Low\n");
		return;
	case RESOURCE_LEVEL_CRITICAL:
		kprintf(LOG_NONE, "Critical\n");
		return;
	}
}

/** Print low resource handler information.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_lrm(int argc, char **argv) {
	lrm_handler_t *handler;
	symbol_t *sym;
	size_t off;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all registered low resource handlers.\n");
		return KDBG_OK;
	} else if(argc != 1) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "Types Priority Function\n");
	kprintf(LOG_NONE, "===== ======== ========\n");

	LIST_FOREACH(&lrm_handlers, iter) {
		handler = list_entry(iter, lrm_handler_t, header);

		kprintf(LOG_NONE, "0x%-3" PRIx32 " %-8" PRIu32 " ", handler->types, handler->priority);
		sym = symbol_lookup_addr((ptr_t)handler->func, &off);
		kprintf(LOG_NONE, "[%p] %s+0x%zx\n", handler->func,
		        (sym) ? sym->name : "<unknown>", off);
	}

	kprintf(LOG_NONE, "\nResource states\n");
	kprintf(LOG_NONE, "===============\n");
	print_resource_state(RESOURCE_TYPE_MEMORY, "Physical Memory:");
	print_resource_state(RESOURCE_TYPE_KASPACE, "Kernel Address Space:");
	return KDBG_OK;
}

/** Perform LRM initialisation. */
__init_text void lrm_init(void) {
	status_t ret;

	/* Create the LRM thread. */
	ret = thread_create("lrm", NULL, 0, lrm_thread_func, NULL, NULL, NULL, &lrm_thread);
	if(ret != STATUS_SUCCESS) {
		fatal("Failed to create LRM thread: %d\n", ret);
	}
	thread_run(lrm_thread);
}
