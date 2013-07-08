/*
 * Copyright (C) 2008-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Process management functions.
 */

#include <arch/memory.h>

#include <lib/id_allocator.h>
#include <lib/string.h>

#include <io/fs.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/futex.h>
#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <elf.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Structure containing process creation information. */
typedef struct process_create {
	/** Arguments provided by the caller. */
	const char *path;		/**< Path to program. */
	const char *const *args;	/**< Argument array. */
	const char *const *env;		/**< Environment array. */
	handle_t (*map)[2];		/**< Handle mapping array. */
	int count;			/**< Number of handles in the array. */

	/** Information used internally by the loader. */
	struct vm_aspace *aspace;	/**< Address space for the process. */
	void *data;			/**< Data pointer for the ELF loader. */
	int argc;			/**< Argument count. */
	int envc;			/**< Environment variable count. */
	ptr_t arg_block;		/**< Address of argument block mapping. */
	ptr_t stack;			/**< Address of stack mapping. */

	/** Information to return to the caller. */
	semaphore_t sem;		/**< Semaphore to wait for completion on. */
	int status;			/**< Status code to return from the call. */
} process_create_t;

extern void kern_process_loaded(void);
static object_type_t process_object_type;

/** Tree of all processes. */
static AVL_TREE_DECLARE(process_tree);
static RWLOCK_DECLARE(process_tree_lock);

/** Process ID allocator. */
static id_allocator_t process_id_allocator;

/** Cache for process structures. */
static slab_cache_t *process_cache;

/** Handle to the kernel library. */
static object_handle_t *kernel_library = NULL;

/** Process containing all kernel-mode threads. */
process_t *kernel_proc = NULL;

/** Constructor for process objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void process_ctor(void *obj, void *data) {
	process_t *process = (process_t *)obj;

	mutex_init(&process->lock, "process_lock", 0);
	list_init(&process->threads);
	avl_tree_init(&process->futexes);
	notifier_init(&process->death_notifier, process);
}

/** Free a process' resources.
 * @note		Safe to call multiple times.
 * @param process	Process to clean up. */
static void process_cleanup(process_t *process) {
	//futex_cleanup(process);
	if(process->aspace) {
		vm_aspace_destroy(process->aspace);
		process->aspace = NULL;
	}
	if(process->handles) {
		handle_table_destroy(process->handles);
		io_context_destroy(&process->ioctx);
		process->handles = NULL;
	}
}

/** Destroy a process structure.
 * @param process	Process to destroy. */
static void process_destroy(process_t *process) {
	assert(refcount_get(&process->count) == 0);
	assert(list_empty(&process->threads));

	rwlock_write_lock(&process_tree_lock);
	avl_tree_remove(&process_tree, &process->tree_link);
	rwlock_unlock(&process_tree_lock);

	dprintf("process: destroyed process %" PRId32 "(%s) (process: %p, status: %d)\n",
		process->id, process->name, process, process->status);

	process_cleanup(process);
	notifier_clear(&process->death_notifier);
	object_destroy(&process->obj);
	id_allocator_free(&process_id_allocator, process->id);
	kfree(process->name);
	slab_cache_free(process_cache, process);
}

/** Decrease the reference count of a process and destroy it if necessary.
 * @param process	Process to decrease count of. */
static void process_release(process_t *process) {
	if(refcount_dec(&process->count) == 0)
		process_destroy(process);
}

/** Closes a handle to a process.
 * @param handle	Handle to close. */
static void process_object_close(object_handle_t *handle) {
	process_release((process_t *)handle->object);
}

/** Signal that a process is being waited for.
 * @param handle	Handle to process.
 * @param event		Event to wait for.
 * @param wait		Internal wait data pointer.
 * @return		Status code describing result of the operation. */
static status_t process_object_wait(object_handle_t *handle, int event, void *wait) {
	process_t *process = (process_t *)handle->object;

	switch(event) {
	case PROCESS_EVENT_DEATH:
		mutex_lock(&process->lock);

		if(process->state == PROCESS_DEAD) {
			object_wait_signal(wait, 0);
		} else {
			notifier_register(&process->death_notifier, object_wait_notifier, wait);
		}

		mutex_unlock(&process->lock);
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a process.
 * @param handle	Handle to process.
 * @param event		Event to wait for.
 * @param wait		Internal wait data pointer. */
static void process_object_unwait(object_handle_t *handle, int event, void *wait) {
	process_t *process = (process_t *)handle->object;

	switch(event) {
	case PROCESS_EVENT_DEATH:
		notifier_unregister(&process->death_notifier, object_wait_notifier, wait);
		break;
	}
}

/** Process object type operations. */
static object_type_t process_object_type = {
	.id = OBJECT_TYPE_PROCESS,
	.flags = OBJECT_TRANSFERRABLE,
	.close = process_object_close,
	.wait = process_object_wait,
	.unwait = process_object_unwait,
};

/**
 * Allocate a process structure and initialize it.
 *
 * Allocates a new process structure and initializes it. If uhandlep is not
 * NULL, then a handle to the process will be created in the parent, and it
 * will be placed in the location(s) specified.
 *
 * @param name		Name to give the process.
 * @param flags		Behaviour flags for the process.
 * @param priority	Priority class to give the process.
 * @param aspace	Address space for the process.
 * @param table		If not NULL, this handle table will be used, otherwise
 *			a new table will be created containing either all
 *			inheritable handles from the parent, or handle mappings
 *			specified in the mapping array.
 * @param parent	Process to inherit information from.
 * @param cflags	Creation flags for the process.
 * @param map		Array of handle mappings (see handle_table_init()).
 * @param count		Number of handles in mapping array.
 * @param procp		Where to store pointer to structure.
 * @param handlep	Where to store handle to process. This is the same as
 *			the ID that will be stored in the userspace location,
 *			and can be used to detach the handle upon failure in
 *			the caller. Handle will not be created unless uhandlep
 *			is not NULL.
 * @param uhandlep	Userspace location to store handle to process in (can
 *			be NULL).
 *
 * @return		Status code describing result of the operation. Note
 *			that on failure the supplied address space will NOT be
 *			destroyed.
 */
static status_t process_alloc(const char *name, int flags, int priority,
	vm_aspace_t *aspace, handle_table_t *table, process_t *parent, int cflags,
	handle_t map[][2], int count, process_t **procp, handle_t *handlep,
	handle_t *uhandlep)
{
	process_t *process;
	object_handle_t *handle;
	status_t ret;

	assert(name);
	assert(procp);

	/* Allocate a new process structure. */
	process = slab_cache_alloc(process_cache, MM_KERNEL);

	/* Attempt to allocate a process ID. If creating the kernel process,
	 * always give it an ID of 0. */
	if(kernel_proc) {
		process->id = id_allocator_alloc(&process_id_allocator);
		if(process->id < 0) {
			slab_cache_free(process_cache, process);
			return STATUS_PROCESS_LIMIT;
		}
	} else {
		process->id = 0;
	}

	/* Create a new handle table for the process if necessary. */
	if(!table) {
		ret = handle_table_create((parent) ? parent->handles : NULL, map, count, &table);
		if(ret != STATUS_SUCCESS) {
			if(kernel_proc)
				id_allocator_free(&process_id_allocator, process->id);

			slab_cache_free(process_cache, process);
			return ret;
		}
	}

	/* Initialize the remainder of the structure. */
	object_init(&process->obj, &process_object_type);
	refcount_set(&process->count, (uhandlep) ? 1 : 0);
	io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	process->flags = flags;
	process->priority = priority;
	process->aspace = aspace;
	process->handles = table;
	process->state = PROCESS_RUNNING;
	process->name = kstrdup(name, MM_KERNEL);
	process->status = 0;
	process->reason = EXIT_REASON_NORMAL;
	process->create = NULL;

	/* Handle the PROCESS_CREATE_CLONE flag. When this is specified, we
	 * inherit signal handling information from the parent. */
	if(cflags & PROCESS_CREATE_CLONE) {
		assert(parent);
		memcpy(process->signal_act, parent->signal_act, sizeof(process->signal_act));
		process->signal_mask = parent->signal_mask;
	} else {
		memset(process->signal_act, 0, sizeof(process->signal_act));
		process->signal_mask = 0;
	}

	/* Add to the process tree. */
	rwlock_write_lock(&process_tree_lock);
	avl_tree_insert(&process_tree, process->id, &process->tree_link);
	rwlock_unlock(&process_tree_lock);

	/* Create a handle to the process if required. */
	if(uhandlep) {
		// FIXME: just remove parent arg?
		assert(parent == curr_proc);

		handle = object_handle_create(&process->obj, NULL, 0);
		ret = object_handle_attach(handle, handlep, uhandlep);
		if(ret != STATUS_SUCCESS) {
			process->aspace = NULL;
			object_handle_release(handle);
			return ret;
		}

		object_handle_release(handle);
	}

	dprintf("process: created process %" PRId32 "(%s) (proc: %p)\n",
		process->id, process->name, process);
	*procp = process;
	return STATUS_SUCCESS;
}

/** Set up a new address space for a process.
 * @param info		Pointer to information structure.
 * @return		Status code describing result of the operation. */
static status_t process_aspace_create(process_create_t *info) {
	object_handle_t *handle;
	status_t ret;
	size_t size;

	semaphore_init(&info->sem, "process_create_sem", 0);
	info->aspace = vm_aspace_create();

	/* Reserve space for the binary being loaded in the address space. The
	 * actual loading of it is done by the kernel library's loader, however
	 * we must reserve space to ensure that the mappings we create below
	 * for the arguments/stack don't end up placed where the binary wants
	 * to be. */
	ret = file_open(info->path, FILE_RIGHT_READ | FILE_RIGHT_EXECUTE, 0, 0, &handle);
	if(ret != STATUS_SUCCESS)
		goto fail;

	ret = elf_binary_reserve(handle, info->aspace);
	object_handle_release(handle);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* If the kernel library has not been opened, open it now. We keep a
	 * handle to it open all the time so that if it gets replaced by a new
	 * version, the new version won't actually be used until the system is
	 * rebooted. This avoids problems if a new kernel is not ABI-compatible
	 * with the previous kernel. */
	if(!kernel_library) {
		ret = file_open(LIBKERNEL_PATH, FILE_RIGHT_READ | FILE_RIGHT_EXECUTE,
			0, 0, &kernel_library);
		if(ret != STATUS_SUCCESS)
			fatal("Could not open kernel library (%d)", ret);
	}

	/* Map the kernel library. */
	ret = elf_binary_load(kernel_library, info->aspace, LIBKERNEL_BASE, &info->data);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Determine the size of the argument block. */
	size = sizeof(process_args_t) + (sizeof(char *) * 2) + strlen(info->path);
	for(info->argc = 0;
		info->args[info->argc];
		size += (strlen(info->args[info->argc++]) + sizeof(char *)));
	for(info->envc = 0;
		info->env[info->envc];
		size += (strlen(info->env[info->envc++]) + sizeof(char *)));
	size = ROUND_UP(size, PAGE_SIZE);

	/* Create a mapping for it. */
	ret = vm_map(info->aspace, 0, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE,
		NULL, 0, &info->arg_block);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Create a stack mapping. */
	ret = vm_map(info->aspace, 0, USTACK_SIZE,
		VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE | VM_MAP_STACK,
		NULL, 0, &info->stack);
	if(ret != STATUS_SUCCESS)
		goto fail;

	return STATUS_SUCCESS;
fail:
	vm_aspace_destroy(info->aspace);
	info->aspace = NULL;
	return ret;
}

/** Copy the data contained in a string array to the argument block.
 * @param dest		Array to store addresses copied to in.
 * @param source	Array to copy data of.
 * @param count		Number of array entries.
 * @param base		Base address to copy to.
 * @return		Total size copied. */
static size_t copy_argument_strings(char **dest, const char *const source[],
	size_t count, ptr_t base)
{
	size_t i, len, total = 0;

	for(i = 0; i < count; i++) {
		dest[i] = (char *)(base + total);
		len = strlen(source[i]) + 1;
		memcpy(dest[i], source[i], len);
		total += len;
	}

	dest[count] = NULL;
	return total;
}

/** Entry thread for a new process.
 * @param arg1		Pointer to creation information structure.
 * @param arg2		Unused. */
static void process_entry_thread(void *arg1, void *arg2) {
	process_create_t *info = arg1;
	ptr_t addr, stack, entry;
	process_args_t *uargs;

	assert(curr_aspace == info->aspace);

	/* Fill out the argument block. */
	addr = info->arg_block;
	uargs = (process_args_t *)addr;
	addr += sizeof(process_args_t);
	uargs->path = (char *)addr;
	addr += strlen(info->path) + 1;
	uargs->args = (char **)addr;
	addr += (info->argc + 1) * sizeof(char *);
	uargs->env = (char **)addr;
	addr += (info->envc + 1) * sizeof(char *);
	uargs->args_count = info->argc;
	uargs->env_count = info->envc;
	uargs->load_base = (void *)LIBKERNEL_BASE;

	/* Copy path string, arguments and environment variables. */
	strcpy(uargs->path, info->path);
	addr += copy_argument_strings(uargs->args, info->args, info->argc, addr);
	copy_argument_strings(uargs->env, info->env, info->envc, addr);

	/* Get the stack pointer and save the argument block pointer. */
	stack = info->stack + USTACK_SIZE;
	addr = info->arg_block;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(info->data);

	/* If there the information structure pointer is NULL, process_replace()
	 * is being used and we don't need to wait for the loader to complete. */
	if(!curr_proc->create)
		semaphore_up(&info->sem, 1);

	/* To userspace, and beyond! */
	dprintf("process: entering userspace in new process (entry: %p, stack: %p, args: %p)\n",
		entry, stack, addr);
	arch_thread_enter_userspace(entry, stack, addr);
	fatal("Failed to enter userspace!");
}

/** Attach a thread to a process.
 * @param process	Process to attach to.
 * @param thread	Thread to attach. */
void process_attach(process_t *process, thread_t *thread) {
	thread->owner = process;

	mutex_lock(&process->lock);

	assert(process->state != PROCESS_DEAD);
	list_append(&process->threads, &thread->owner_link);
	refcount_inc(&process->count);

	mutex_unlock(&process->lock);
}

/** Detach a thread from its owner.
 * @param thread	Thread to detach. */
void process_detach(thread_t *thread) {
	process_t *process = thread->owner;

	mutex_lock(&process->lock);
	list_remove(&thread->owner_link);

	/* Move the process to the dead state if it contains no more threads. */
	if(list_empty(&process->threads)) {
		assert(process->state != PROCESS_DEAD);
		process->state = PROCESS_DEAD;
		process_cleanup(process);

		/* If the create info pointer is not NULL, a process_create()
		 * call is waiting. Make it return with the process' exit code. */
		if(process->create) {
			process->create->status = process->status;
			semaphore_up(&process->create->sem, 1);
			process->create = NULL;
		} else if(process->flags & PROCESS_CRITICAL && !shutdown_in_progress) {
			fatal("Critical process %" PRId32 "(%s) terminated", process->id, process->name);
		}

		mutex_unlock(&process->lock);

		/* Don't bother running callbacks during shutdown. */
		if(!shutdown_in_progress)
			notifier_run(&process->death_notifier, NULL, true);
	} else {
		mutex_unlock(&process->lock);
	}

	thread->owner = NULL;
	process_release(process);
}

/** Look up a process without taking the tree lock.
 * @note		This function should only be used within KDB. Use
 *			process_lookup() outside of KDB.
 * @param id		ID of the process to find.
 * @return		Pointer to process found, or NULL if not found. */
process_t *process_lookup_unsafe(process_id_t id) {
	return avl_tree_lookup(&process_tree, id, process_t, tree_link);
}

/** Look up a process.
 * @param id		ID of the process to find.
 * @return		Pointer to process found, or NULL if not found. */
process_t *process_lookup(process_id_t id) {
	process_t *ret;

	rwlock_read_lock(&process_tree_lock);
	ret = process_lookup_unsafe(id);
	rwlock_unlock(&process_tree_lock);
	return ret;
}

/**
 * Execute a new process.
 *
 * Creates a new process and runs a program within it. The path to the process
 * should be the first argument specified in the argument structure. The new
 * process will inherit no handles from the parent.
 *
 * @param args		Arguments to pass to process (NULL-terminated array).
 * @param env		Environment to pass to process (NULL-terminated array).
 * @param flags		Behaviour flags for the process.
 * @param priority	Priority class for the process.
 * @param parent	Parent for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		Status code describing result of the operation.
 */
status_t process_create(const char *const args[], const char *const env[], int flags,
	int priority, process_t *parent, process_t **procp)
{
	process_create_t info;
	process_t *process;
	thread_t *thread;
	status_t ret;

	if(!args || !args[0] || !env || priority < 0 || priority > PRIORITY_CLASS_MAX)
		return STATUS_INVALID_ARG;

	info.path = args[0];
	info.args = args;
	info.env = env;

	/* Create the address space for the process. */
	ret = process_aspace_create(&info);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Create the new process. */
	ret = process_alloc(args[0], flags, priority, info.aspace, NULL, parent,
		0, NULL, 0, &process, NULL, NULL);
	if(ret != STATUS_SUCCESS) {
		vm_aspace_destroy(info.aspace);
		return ret;
	}

	/* Create and run the entry thread. */
	ret = thread_create("main", process, 0, process_entry_thread, &info, NULL, &thread);
	if(ret != STATUS_SUCCESS) {
		process_destroy(process);
		return ret;
	}

	process->create = &info;
	thread_run(thread);
	thread_release(thread);

	/* Wait for the process to finish loading. */
	semaphore_down(&info.sem);
	if(procp)
		*procp = process;

	return info.status;
}

/**
 * Terminate the calling process.
 *
 * Terminates the calling process. All threads in the process will also be
 * terminated. The status and reason codes given can be retrieved by any
 * processes with a handle to the process with kern_process_status().
 *
 * @param status	Exit status code.
 * @param reason	Exit reason.
 */
void process_exit(int status, int reason) {
	thread_t *thread;

	mutex_lock(&curr_proc->lock);

	LIST_FOREACH_SAFE(&curr_proc->threads, iter) {
		thread = list_entry(iter, thread_t, owner_link);

		if(thread != curr_thread)
			thread_kill(thread);
	}

	curr_proc->status = status;
	curr_proc->reason = reason;
	mutex_unlock(&curr_proc->lock);

	thread_exit();
}

/** Dump the contents of the process tree.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_process(int argc, char **argv, kdb_filter_t *filter) {
	process_t *process;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints a list of all running processes.\n");
		return KDB_SUCCESS;
	}

	kdb_printf("ID     State   Prio Flags Count Aspace             Name\n");
	kdb_printf("==     =====   ==== ===== ===== ======             ====\n");

	AVL_TREE_FOREACH(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t, tree_link);

		kdb_printf("%-5" PRId32 "%s ", process->id,
			(process == curr_proc) ? "*" : " ");

		switch(process->state) {
		case PROCESS_RUNNING:	kdb_printf("Running "); break;
		case PROCESS_DEAD:	kdb_printf("Dead    "); break;
		default:		kdb_printf("Bad     "); break;
		}

		kdb_printf("%-4d %-5d %-5d %-18p %s\n", process->priority,
			process->flags, refcount_get(&process->count),
			process->aspace, process->name);
	}

	return KDB_SUCCESS;;
}

/** Initialize the process table and slab cache. */
__init_text void process_init(void) {
	status_t ret;

	/* Create the process ID allocator. We reserve ID 0 as it is always
	 * given to the kernel process. */
	id_allocator_init(&process_id_allocator, 65535, MM_BOOT);
	id_allocator_reserve(&process_id_allocator, 0);

	/* Create the process slab cache. */
	process_cache = slab_cache_create("process_cache", SLAB_SIZE_ALIGN(process_t),
		process_ctor, NULL, NULL, 0, MM_BOOT);

	/* Register the KDB command. */
	kdb_register_command("process", "Print a list of running processes.", kdb_cmd_process);

	/* Create the kernel process. */
	ret = process_alloc("[kernel]", PROCESS_CRITICAL, PRIORITY_CLASS_SYSTEM,
		NULL, NULL, NULL, 0, NULL, 0, &kernel_proc, NULL, NULL);
	if(ret != STATUS_SUCCESS)
		fatal("Could not initialize kernel process (%d)", ret);
}

/** Terminate all running processes. */
void process_shutdown(void) {
	nstime_t interval = 0;
	process_t *process;
	thread_t *thread;
	int count;

	rwlock_read_lock(&process_tree_lock);

	AVL_TREE_FOREACH_SAFE(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t, tree_link);
		if(process != kernel_proc) {
			LIST_FOREACH_SAFE(&process->threads, titer) {
				thread = list_entry(titer, thread_t, owner_link);
				thread_kill(thread);
			}
		}
	}

	rwlock_unlock(&process_tree_lock);

	/* Wait until everything has terminated. */
	do {
		delay(MSECS2NSECS(1));
		interval += MSECS2NSECS(1);

		count = 0;
		rwlock_read_lock(&process_tree_lock);
		AVL_TREE_FOREACH_SAFE(&process_tree, iter) {
			process = avl_tree_entry(iter, process_t, tree_link);
			if(process != kernel_proc) {
				count++;
				if(!(interval % SECS2NSECS(2)) && process->state == PROCESS_RUNNING) {
					kprintf(LOG_NOTICE, "system: still waiting for %u(%s)...\n",
						process->id, process->name);
				}
			}
		}
		rwlock_unlock(&process_tree_lock);
	} while(count);

	/* Close the kernel library handle. */
	object_handle_release(kernel_library);
}

/** Helper to free information copied from userspace.
 * @note		Does not free the address space.
 * @param info		Structure containing copied information. */
static void process_create_args_free(process_create_t *info) {
	int i;

	if(info->path)
		kfree((void *)info->path);

	if(info->args) {
		for(i = 0; info->args[i]; i++)
			kfree((void *)info->args[i]);

		kfree((void *)info->args);
	}

	if(info->env) {
		for(i = 0; info->env[i]; i++)
			kfree((void *)info->env[i]);

		kfree((void *)info->env);
	}

	if(info->map)
		kfree(info->map);
}

/** Helper to copy process creation information from userspace.
 * @param path		Path to copy.
 * @param args		Argument array to copy.
 * @param env		Environment array to copy.
 * @param map		Handle array to copy.
 * @param count		Size of handle array.
 * @param info		Pointer to information structure to fill in.
 * @return		Status code describing result of the operation. */
static status_t process_create_args_copy(const char *path, const char *const args[],
	const char *const env[], handle_t map[][2],
	int count, process_create_t *info)
{
	status_t ret;
	size_t size;

	if(!path || !args || !env || (count > 0 && !map))
		return STATUS_INVALID_ARG;

	info->path = NULL;
	info->args = NULL;
	info->env = NULL;
	info->map = NULL;
	info->aspace = NULL;

	ret = strndup_from_user(path, FS_PATH_MAX, (char **)&info->path);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = arrcpy_from_user(args, (char ***)&info->args);
	if(ret != STATUS_SUCCESS) {
		process_create_args_free(info);
		return ret;
	}

	ret = arrcpy_from_user(env, (char ***)&info->env);
	if(ret != STATUS_SUCCESS) {
		process_create_args_free(info);
		return ret;
	}

	if(count > 0) {
		size = sizeof(handle_t) * 2 * count;
		info->map = kmalloc(size, MM_USER);
		if(!info->map) {
			process_create_args_free(info);
			return STATUS_NO_MEMORY;
		}

		ret = memcpy_from_user(info->map, map, size);
		if(ret != STATUS_SUCCESS) {
			process_create_args_free(info);
			return ret;
		}
	}

	return STATUS_SUCCESS;
}

/**
 * Create a new process.
 *
 * Creates a new process and executes a program within it. If the count
 * argument is negative, then all handles with the HANDLE_INHERITABLE flag in
 * the calling process will be duplicated into the child process with the same
 * IDs. If it is 0, no handles will be duplicated to the child process.
 * Otherwise, handles will be duplicated according to the given array.
 *
 * @param path		Path to binary to load.
 * @param args		Array of arguments to pass to the program
 *			(NULL-terminated).
 * @param env		Array of environment variables for the program
 *			(NULL-terminated).
 * @param flags		Flags modifying creation behaviour.
 * @param map		Array containing handle mappings (can be NULL if count
 *			is less than or equal to 0). The first ID of each entry
 *			specifies the handle in the caller, and the second
 *			specifies the ID to give it in the child.
 * @param count		Number of entries in handle mapping array, or -1 if
 *			array should not be used.
 * @param handlep	Where to store handle to process (can be NULL).
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_process_create(const char *path, const char *const args[],
	const char *const env[], int flags, handle_t map[][2], int count,
	handle_t *handlep)
{
	process_t *process = NULL;
	process_create_t info;
	thread_t *thread;
	handle_t handle;
	status_t ret;

	ret = process_create_args_copy(path, args, env, map, count, &info);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Create the address space for the process. */
	ret = process_aspace_create(&info);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Create the new process and a handle to it. */
	ret = process_alloc(info.path, 0, PRIORITY_CLASS_NORMAL, info.aspace, NULL,
		curr_proc, flags, info.map, count, &process, &handle, handlep);
	if(ret != STATUS_SUCCESS)
		goto fail;

	process->create = &info;

	/* Create and run the entry thread. */
	ret = thread_create("main", process, 0, process_entry_thread, &info, NULL, &thread);
	if(ret != STATUS_SUCCESS)
		goto fail;

	process->create = &info;
	thread_run(thread);
	thread_release(thread);

	/* Wait for the process to finish loading. */
	semaphore_down(&info.sem);
	ret = info.status;
	if(ret != STATUS_SUCCESS)
		goto fail;

	process_create_args_free(&info);
	return ret;
fail:
	if(process) {
		if(handlep) {
			object_handle_detach(handle);
		} else {
			process_destroy(process);
		}
	} else if(info.aspace) {
		vm_aspace_destroy(info.aspace);
	}

	process_create_args_free(&info);
	return ret;
}

/**
 * Replace the current process.
 *
 * Replaces the current process with a new program. All threads in the process
 * other than the calling thread will be terminated. If the count argument is
 * negative, then all handles with the HANDLE_INHERITABLE flag in the process
 * will remain open with the same IDs. If it is 0, all of the process' handles
 * will be closed. Otherwise, handles will be made available in the new process
 * according to the given array.
 *
 * @param path		Path to binary to load.
 * @param args		Array of arguments to pass to the program
 *			(NULL-terminated).
 * @param env		Array of environment variables for the program
 *			(NULL-terminated).
 * @param map		Array containing handle mappings (can be NULL if count
 *			is less than or equal to 0). The first ID of each entry
 *			specifies the handle in the caller, and the second
 *			specifies the ID to move it to.
 * @param count		Number of entries in handle mapping array, or -1 if
 *			array should not be used.
 *
 * @return		Does not return on success, returns status code on
 *			failure.
 */
status_t kern_process_replace(const char *path, const char *const args[],
	const char *const env[], handle_t map[][2], int count)
{
	handle_table_t *table, *oldtable;
	thread_t *thread = NULL;
	process_create_t info;
	vm_aspace_t *oldas;
	char *oldname;
	status_t ret;

	if(curr_proc->threads.next->next != &curr_proc->threads) {
		kprintf(LOG_WARN, "TODO: Terminate other threads\n");
		return STATUS_NOT_IMPLEMENTED;
	}

	ret = process_create_args_copy(path, args, env, map, count, &info);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Create the new address space for the process. */
	ret = process_aspace_create(&info);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Create the entry thread to finish loading the program. */
	ret = thread_create("main", curr_proc, 0, process_entry_thread, &info,
		NULL, &thread);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Create a new handle table for the process. */
	ret = handle_table_create(curr_proc->handles, info.map, count, &table);
	if(ret != STATUS_SUCCESS)
		goto fail;

	mutex_lock(&curr_proc->lock);

	/* Switch over to the new address space and handle table. */
	thread_disable_preempt();
	vm_aspace_switch(info.aspace);
	oldas = curr_proc->aspace;
	curr_proc->aspace = info.aspace;
	thread_enable_preempt();
	oldtable = curr_proc->handles;
	curr_proc->handles = table;
	oldname = curr_proc->name;
	curr_proc->name = kstrdup(info.path, MM_KERNEL);

	/* Reset signal handling state. */
	memset(curr_proc->signal_act, 0, sizeof(curr_proc->signal_act));
	curr_proc->signal_mask = 0;
	curr_thread->signal_stack.ss_sp = NULL;
	curr_thread->signal_stack.ss_size = 0;
	curr_thread->signal_stack.ss_flags = SS_DISABLE;

	mutex_unlock(&curr_proc->lock);

	/* Free up old data. */
	vm_aspace_destroy(oldas);
	handle_table_destroy(oldtable);
	kfree(oldname);

	/* Run the thread and wait for it to complete, then free up data and
	 * exit this thread. */
	thread_run(thread);
	thread_release(thread);
	semaphore_down(&info.sem);
	process_create_args_free(&info);
	thread_exit();
fail:
	if(thread)
		thread_release(thread);
	if(info.aspace)
		vm_aspace_destroy(info.aspace);

	process_create_args_free(&info);
	return ret;
}

/**
 * Clone the calling process.
 *
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to the pages. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process will inherit all
 * handles from the parent, including non-inheritable ones. Threads, however,
 * are not cloned: the new process will have a single thread which will begin
 * execution at the address specified on the specified stack.
 *
 * @param func		Where to begin execution at in the new process.
 * @param arg		Argument to pass to entry function.
 * @param sp		Stack pointer to use. Depending on the architecture,
 *			this may need to have space to store the argument to
 *			the function.
 * @param handlep	Where to store handle to the child process.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_process_clone(void (*func)(void *), void *arg, void *sp, handle_t *handlep) {
	thread_uspace_args_t *args;
	process_t *process = NULL;
	handle_table_t *table;
	thread_t *thread;
	vm_aspace_t *as;
	handle_t handle;
	status_t ret;

	if(validate_user_address(sp, 0) != STATUS_SUCCESS)
		return STATUS_INVALID_ADDR;

	/* Create a clone of the process' address space and handle table. */
	as = vm_aspace_clone(curr_proc->aspace);
	table = handle_table_clone(curr_proc->handles);

	/* Create the new process and a handle to it. */
	ret = process_alloc(curr_proc->name, 0, PRIORITY_CLASS_NORMAL, as, table,
		curr_proc, PROCESS_CREATE_CLONE, NULL, 0, &process, &handle,
		handlep);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Create the entry thread. */
	args = kmalloc(sizeof(*args), MM_KERNEL);
	args->entry = (ptr_t)func;
	args->arg = (ptr_t)arg;
	args->sp = (ptr_t)sp;
	ret = thread_create("main", process, 0, thread_uspace_trampoline, args,
		NULL, &thread);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Inherit certain per-thread attributes such as the alternate signal
	 * stack and TLS address from the thread that called us. */
	memcpy(&thread->signal_stack, &curr_thread->signal_stack, sizeof(thread->signal_stack));
	arch_thread_set_tls_addr(thread, arch_thread_tls_addr(curr_thread));

	/* Run the new thread. */
	thread_run(thread);
	thread_release(thread);
	return STATUS_SUCCESS;
fail:
	if(process) {
		if(handlep) {
			object_handle_detach(handle);
		} else {
			process_destroy(process);
		}
	} else {
		handle_table_destroy(table);
		vm_aspace_destroy(as);
	}

	return ret;
}

/** Open a handle to a process.
 * @param id		ID of the process to open.
 * @param handlep	Where to store handle to process.
 * @return		Status code describing result of the operation. */
status_t kern_process_open(process_id_t id, handle_t *handlep) {
	process_t *process;
	object_handle_t *handle;
	status_t ret;

	if(!handlep)
		return STATUS_INVALID_ARG;

	rwlock_read_lock(&process_tree_lock);

	process = process_lookup_unsafe(id);
	if(!process) {
		rwlock_unlock(&process_tree_lock);
		return STATUS_NOT_FOUND;
	} else if(process->state == PROCESS_DEAD || process == kernel_proc) {
		rwlock_unlock(&process_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&process->count);
	rwlock_unlock(&process_tree_lock);

	handle = object_handle_create(&process->obj, NULL, 0);
	ret = object_handle_attach(handle, NULL, handlep);
	object_handle_release(handle);
	return ret;
}

/** Get the ID of a process.
 * @param handle	Handle for process to get ID of, or -1 to get ID of
 *			the calling process.
 * @return		Process ID on success, -1 if handle is invalid. */
process_id_t kern_process_id(handle_t handle) {
	object_handle_t *khandle;
	process_id_t id = -1;
	process_t *process;

	if(handle < 0) {
		id = curr_proc->id;
	} else if(object_handle_lookup(handle, OBJECT_TYPE_PROCESS, 0, &khandle) == STATUS_SUCCESS) {
		process = (process_t *)khandle->object;
		id = process->id;
		object_handle_release(khandle);
	}

	return id;
}

/** Perform operations on a process.
 * @param handle	Handle to process, or -1 to operate on the calling
 *			process.
 * @param action	Action to perform.
 * @param in		Pointer to input buffer.
 * @param out		Pointer to output buffer.
 * @return		Status code describing result of the operation. */
status_t kern_process_control(handle_t handle, int action, const void *in, void *out) {
	object_handle_t *khandle = NULL;
	process_t *process;
	status_t ret;

	if(handle < 0) {
		process = curr_proc;
	} else {
		ret = object_handle_lookup(handle, OBJECT_TYPE_PROCESS, 0, &khandle);
		if(ret != STATUS_SUCCESS)
			return ret;

		process = (process_t *)khandle->object;
	}

	switch(action) {
	case PROCESS_LOADED:
		if(khandle) {
			ret = STATUS_NOT_SUPPORTED;
			goto out;
		}

		mutex_lock(&process->lock);
		if(process->create) {
			process->create->status = STATUS_SUCCESS;
			semaphore_up(&process->create->sem, 1);
			process->create = NULL;
		}
		mutex_unlock(&curr_proc->lock);
		break;
	}
out:
	if(khandle) {
		object_handle_release(khandle);
	}
	return ret;
}

/** Query the exit status of a process.
 * @param handle	Handle to process.
 * @param statusp	Where to store exit status of process.
 * @param reasonp	Where to store exit reason.
 * @return		Status code describing result of the operation. */
status_t kern_process_status(handle_t handle, int *statusp, int *reasonp) {
	object_handle_t *khandle;
	process_t *process;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_PROCESS, 0, &khandle);
	if(ret != STATUS_SUCCESS)
		return ret;

	process = (process_t *)khandle->object;

	if(process->state != PROCESS_DEAD) {
		object_handle_release(khandle);
		return STATUS_STILL_RUNNING;
	}

	if(statusp)
		ret = memcpy_to_user(statusp, &process->status, sizeof(*statusp));

	if(reasonp)
		ret = memcpy_to_user(reasonp, &process->reason, sizeof(*reasonp));

	object_handle_release(khandle);
	return ret;
}

/**
 * Terminate the calling process.
 *
 * Terminates the calling process. All threads in the process will also be
 * terminated. The status code given can be retrieved by any processes with a
 * handle to the process.
 *
 * @param status	Exit status code.
 */
void kern_process_exit(int status) {
	process_exit(status, EXIT_REASON_NORMAL);
}
