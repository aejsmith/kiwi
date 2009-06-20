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

#include <mm/malloc.h>

#include <proc/uspace.h>

#include <assert.h>

#include "loader_priv.h"

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
 * To perform the second and third steps, process_reset() is called. If the
 * new binary runs under the same subsystem as the old binary, then this will
 * call the process_reset callback for the subsystem. Otherwise, it calls
 * process_destroy for the old subsystem, and process_init for the new one.
 * This allows, for example, the POSIX subsystem to preserve file descriptors
 * across an execve() call for another POSIX binary.
 *
 * When successful, this function does not return to the calling kernel
 * function. This means that several assumptions must be made about the
 * arguments it is passed. It will release the filesystem node given, and both
 * the contents of the arrays given and the arrays themselves will be freed.
 * If it is not successful, it is up to the caller to release the node and
 * free the arrays.
 *
 * @param node		Node referring to the binary to load.
 * @param args		Arguments to pass to the new process (how these are
 *			passed in, or whether they are passed at all, are
 *			dependent on the subsystem in use). This can be NULL.
 * @param environ	Environment variables for the new process (same rules
 *			apply as for arguments). This can be NULL.
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
		ret = -ERR_OBJ_TYPE_INVAL;
		goto fail;
	}

	/* Create a new address space for the binary-> */
	binary->aspace = aspace_create();
	if(binary->aspace == NULL) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	}	

	/* Now get the binary type to map the binary's data into the address
	 * space. This should also get us the subsystem and entry pointers. */
	ret = binary->type->load(binary);
	if(ret != 0) {
		goto fail;
	}

	assert(binary->entry);
	assert(binary->subsystem);

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
	ret = process_reset(curr_proc, binary->node->name, binary->aspace, binary->subsystem);
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
MODULE_EXPORT(loader_binary_load);
