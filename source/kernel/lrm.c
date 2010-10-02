/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Low Resource Manager.
 *
 * The Low Resource Manager is a centralised system for reclaiming resources
 * when the levels of free resources are running low. It allows kernel
 * subsystems (such as the slab allocator and the page cache) to register
 * handler functions to reclaim resources.
 */

#include <mm/page.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/condvar.h>
#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <console.h>
#include <fatal.h>
#include <kdbg.h>
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

/** List of registered handlers, ordered with highest priority first. */
static LIST_DECLARE(lrm_handlers);
static SPINLOCK_DECLARE(lrm_handlers_lock);

/** LRM thread. */
static thread_t *lrm_thread = NULL;
static CONDVAR_DECLARE(lrm_request_cvar);
static CONDVAR_DECLARE(lrm_response_cvar);

/** Main function for the LRM thread.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void lrm_thread_func(void *arg1, void *arg2) {
	while(true) {
		/* Wait either for the check interval, or until a call to
		 * lrm_reclaim() requests that we run. */
		condvar_wait_etc(&lrm_request_cvar, NULL, NULL, LRM_INTERVAL, 0);
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

/** Get the level of available resources.
 * @param types		Types to get level of.
 * @return		Level of resource available. This will be the lowest
 *			level out of the levels for all the types specified. */
int lrm_level(uint32_t types) {
	// TODO
	return RESOURCE_LEVEL_OK;
}

/** Attempt to reclaim a resource.
 * @param type		Type of resource to reclaim.
 * @param required	Amount of the resource required. */
void lrm_reclaim(uint32_t type, uint64_t required) {
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

	// TODO
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
void __init_text lrm_init(void) {
	status_t ret;

	/* Create the LRM thread. */
	ret = thread_create("lrm", kernel_proc, 0, lrm_thread_func, NULL, NULL, &lrm_thread);
	if(ret != STATUS_SUCCESS) {
		fatal("Failed to create LRM thread: %d\n", ret);
	}
}
