/* Kiwi executable loader
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
 * @brief		Executable loader.
 */

#include <arch/stack.h>

#include <console/kprintf.h>

#include <mm/malloc.h>

#include <proc/loader.h>
#include <proc/process.h>
#include <proc/thread.h>
#include <proc/uspace.h>

#include <sync/mutex.h>

#include <assert.h>
#include <bootmod.h>
#include <errors.h>
#include <init.h>

#if CONFIG_LOADER_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** List of known binary types. */
static LIST_DECLARE(loader_type_list);
static MUTEX_DECLARE(loader_type_list_lock, 0);

#if 0
# pragma mark Executable type management functions.
#endif

/** Match a binary to a type.
 * @param node		VFS node for the binary.
 * @return		Pointer to type if matched, NULL if not. */
static loader_type_t *loader_type_match(vfs_node_t *node) {
	loader_type_t *type;

	mutex_lock(&loader_type_list_lock, 0);

	LIST_FOREACH(&loader_type_list, iter) {
		type = list_entry(iter, loader_type_t, header);

		if(type->check(node)) {
			mutex_unlock(&loader_type_list_lock);
			return type;
		}
	}

	mutex_unlock(&loader_type_list_lock);
	return NULL;
}

/** Register a binary type.
 *
 * Registers a binary type with the program loader.
 *
 * @param type		Binary type to add.
 */
int loader_type_register(loader_type_t *type) {
	if(!type->name || !type->check || !type->load || !type->finish || !type->cleanup) {
		return -ERR_PARAM_INVAL;
	}

	list_init(&type->header);

	mutex_lock(&loader_type_list_lock, 0);
	list_append(&loader_type_list, &type->header);
	mutex_unlock(&loader_type_list_lock);

	dprintf("loader: registered binary type %p(%s)\n", type, type->name);
	return 0;
}

#if 0
# pragma mark Executable loading functions.
#endif

/** Free an array of strings. Assumes array is NULL-terminated.
 * @param array		Array to free. */
static void array_free(char **array) {
	size_t i;

	for(i = 0; array[i]; i++) {
		kfree(array[i]);
	}

	kfree(array);
}

/** Replace the current process with a new binary.
 *
 * Replaces the current process with a new binary. This is done in several
 * steps.
 *  - Load the binary into a new address space.
 *  - Terminate all threads except the current thread.
 *  - Replace the current address space with the new one.
 *  - Begin executing the new binary.
 * To perform the second and third steps, process_reset() is called.
 *
 * When successful, this function does not return to the calling kernel
 * function. This means that several assumptions must be made about the
 * arguments it is passed. It will release the filesystem node given, and both
 * the contents of the arrays given and the arrays themselves will be freed.
 * If it is not successful, it is up to the caller to release the node and
 * free the arrays.
 *
 * @param node		Node referring to the binary to load.
 * @param args		Arguments to pass to the new process. Can be NULL.
 * @param environ	Environment variables for the new process. Can be NULL.
 * @param sem		If this argument is not NULL, it should point to a
 *			semaphore that will be upped if this function is
 *			successful, just before it enters the new program.
 *			This allows, for example, something to create a new
 *			process which runs this function, and get notified
 *			if it successfully completes.
 *
 * @return		Does not return on success; if it does return it will
 *			be for a failure, in which case the return value will
 *			be a negative error code.
 */
int loader_binary_load(vfs_node_t *node, char **args, char **environ, semaphore_t *sem) {
	aspace_source_t *source;
	loader_binary_t *binary;
	ptr_t stack, entry;
	int ret;

	if(!node) {
		return -ERR_PARAM_INVAL;
	}

	/* Initialize the loader data structure. Use kcalloc() because we
	 * want stuff we don't set here to be zero. */
	binary = kcalloc(1, sizeof(loader_binary_t), MM_SLEEP);
	binary->node = node;
	binary->args = args;
	binary->environ = environ;

	/* Attempt to match the binary to a type. */
	binary->type = loader_type_match(node);
	if(binary->type == NULL) {
		ret = -ERR_TYPE_INVAL;
		goto fail;
	}

	/* Create a new address space for the binary-> */
	binary->aspace = aspace_create();
	if(binary->aspace == NULL) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	}	

	/* Now get the binary type to map the binary's data into the address
	 * space. This should also get us the entry pointers. */
	ret = binary->type->load(binary);
	if(ret != 0) {
		goto fail;
	}

	assert(binary->entry);

	/* Create a userspace stack. Do this now because after this is done we
	 * don't want to fail. */
	ret = aspace_anon_create(AS_SOURCE_PRIVATE, &source);
	if(ret != 0) {
		goto fail;
	}

	ret = aspace_alloc(binary->aspace, USTACK_SIZE, AS_REGION_READ | AS_REGION_WRITE, source, 0, &stack);
	if(ret != 0) {
		aspace_source_destroy(source);
		goto fail;
	}

	binary->stack = stack + USTACK_SIZE;

	/* OK, take the plunge and start messing with the process. If we fail
	 * after this point, then we're done for. */
	ret = process_reset(curr_proc, binary->node->name, binary->aspace);
	if(ret != 0) {
		/* TODO: Proper handling here. */
		fatal("Failed to reset process");
	}

	thread_rename(curr_thread, binary->node->name);

	/* Get the binary type to do anything it needs to do once the address
	 * space has been switched (such as copying arguments). */
	binary->type->finish(binary);

	/* Save the entry point address and updated stack pointer. */
	entry = binary->entry;
	stack = binary->stack;

	/* Clean up state data. */
	binary->type->cleanup(binary);
	kfree(binary);
	if(args) {
		array_free(args);
	}
	if(environ) {
		array_free(environ);
	}
	vfs_node_release(node);

	/* Wake up the semaphore if the caller asked us to. */
	if(sem) {
		semaphore_up(sem);
	}

	/* To userspace, and beyond! */
	dprintf("loader: entering userspace (entry: %p, stack: %p)\n", entry, stack);
	uspace_entry(entry, stack);
	fatal("Returned from uspace_entry!");
fail:
	if(binary->data) {
		binary->type->cleanup(binary);
	}
	if(binary->aspace) {
		aspace_destroy(binary->aspace);
	}
	return ret;
}

#if 0
# pragma mark Core functions.
#endif

/** Thread to load a binary provided at boot.
 * @param arg1		Pointer to VFS node for binary.
 * @param arg2		Pointer to semaphore to up upon successful load. */
static void loader_bootmod_thread(void *arg1, void *arg2) {
	semaphore_t *sem = arg2;
	vfs_node_t *node = arg1;
	int ret;

	ret = loader_binary_load(node, NULL, NULL, sem);
	fatal("Failed to load binary '%s' (%d)", node->name, ret);
}

/** Executable boot module handler.
 * @param mod		Boot module structure.
 * @return		1 if loaded, 0 if module valid but cannot be loaded
 *			yet (dependencies, etc), -1 if module not this type. */
static int loader_bootmod_handler(bootmod_t *mod) {
	SEMAPHORE_DECLARE(semaphore, 0);
	vfs_node_t *node;
	thread_t *thread;
	process_t *proc;
	int ret;

	/* Create a node from the module's data. */
	ret = vfs_node_create_from_memory(mod->name, (const void *)mod->addr, mod->size, &node);
	if(ret != 0) {
		fatal("Could not create VFS node from module data (%d)", ret);
	}

	/* Attempt to match the node to a type. */
	if(!loader_type_match(node)) {
		vfs_node_release(node);
		return -1;
	}

	/* Create process to load the binary. Does not require an address space
	 * to begin with as loader_binary_load() will create one. */
	ret = process_create(mod->name, kernel_proc, PRIORITY_SYSTEM, PROCESS_NOASPACE, &proc);
	if(ret != 0) {
		fatal("Could not create process to load binary (%d)", ret);
	}

	ret = thread_create(mod->name, proc, 0, loader_bootmod_thread, node, &semaphore, &thread);
	if(ret != 0) {
		fatal("Could not create thread to load binary (%d)", ret);
	}
	thread_run(thread);

	/* Wait for the binary to load. The node is released for us by
	 * loader_binary_load(). */
	semaphore_down(&semaphore, 0);
	return 1;
}

/** Initialization function to register a boot module handler. */
static void __init_text loader_init(void) {
	bootmod_handler_register(loader_bootmod_handler);
}
INITCALL(loader_init);
