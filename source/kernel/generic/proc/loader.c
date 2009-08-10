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
#include <errors.h>

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
 * steps:
 *  - Load the binary into a new address space.
 *  - Terminate all threads in the process except the current thread.
 *  - Replace the current address space with the new one.
 *  - Begin executing the new binary.
 * When successful, this function does not return to the calling kernel
 * function. This means that several assumptions must be made about the
 * arguments it is passed. It will free the path string given, and both the
 * the contents of the arrays given and the arrays themselves will be freed.
 * If it is not successful, it is up to the caller to free the path and arrays.
 *
 * @param path		Path to the binary to load.
 * @param args		Arguments to pass to the new process.
 * @param environ	Environment variables for the new process.
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
int loader_binary_load(char *path, char **args, char **environ, semaphore_t *sem) {
	loader_binary_t *binary;
	ptr_t stack, entry;
	int ret;

	if(!path || !args || !environ) {
		return -ERR_PARAM_INVAL;
	}

	binary = kcalloc(1, sizeof(loader_binary_t), MM_SLEEP);
	binary->args = args;
	binary->environ = environ;

	/* Look up the node on the filesystem. */
	if((ret = vfs_node_lookup(path, true, &binary->node)) != 0) {
		goto fail;
	} else if(binary->node->type != VFS_NODE_FILE) {
		ret = -ERR_TYPE_INVAL;
		goto fail;
	}

	/* Attempt to match the binary to a type. */
	if(!(binary->type = loader_type_match(binary->node))) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Create a new address space for the binary. */
	if(!(binary->aspace = vm_aspace_create())) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	}	

	/* Get the binary type to map the binary's data into the address space.
	 * This should set the entry address pointer also get us the entry pointers. */
	if((ret = binary->type->load(binary)) != 0) {
		goto fail;
	}

	/* Create a userspace stack. */
	ret = vm_map_anon(binary->aspace, 0, USTACK_SIZE,
	                  VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE,
	                  &stack);
	if(ret != 0) {
		goto fail;
	}

	binary->stack = stack + USTACK_SIZE;

	/* Take the plunge and start messing with the process. */
	if((ret = process_reset(curr_proc, path, binary->aspace)) != 0) {
		goto fail;
	}

	thread_rename(curr_thread, "main");

	/* Get the binary type to do anything it needs to do once the address
	 * space has been switched (such as copying arguments). */
	binary->type->finish(binary);

	/* Save the entry point address and updated stack pointer. */
	entry = binary->entry;
	stack = binary->stack;

	/* Clean up state data. */
	binary->type->cleanup(binary);
	vfs_node_release(binary->node);
	kfree(binary);
	array_free(args);
	array_free(environ);
	kfree(path);

	/* If the caller provided a semaphore, wake it up. */
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
		vm_aspace_destroy(binary->aspace);
	}
	if(binary->node) {
		vfs_node_release(binary->node);
	}
	kfree(binary);
	return ret;
}
