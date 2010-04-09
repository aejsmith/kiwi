/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Process management functions.
 *
 * @todo		Check execute permission on binaries.
 */

#include <lib/avl_tree.h>
#include <lib/string.h>

#include <io/fs.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/rwlock.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <console.h>
#include <elf.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>
#include <vmem.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Structure containing process creation information. */
typedef struct process_create_info {
	const char *path;		/**< Path to program. */
	const char **args;		/**< Argument array. */
	const char **env;		/**< Environment array. */
	handle_id_t (*handles)[2];	/**< Handle mapping array. */
	int count;			/**< Number of handles in the array. */
	vm_aspace_t *aspace;		/**< Address space for the process. */
	void *data;			/**< Data pointer for the ELF loader. */
	int argc;			/**< Argument count. */
	int envc;			/**< Environment variable count. */
	ptr_t arg_block;		/**< Address of argument block mapping. */
	ptr_t stack;			/**< Address of stack mapping. */
	semaphore_t sem;		/**< Semaphore to wait for completion on. */
} process_create_info_t;

static object_type_t process_object_type;

/** Tree of all processes. */
static AVL_TREE_DECLARE(process_tree);
static RWLOCK_DECLARE(process_tree_lock);

/** Process ID allocator. */
static vmem_t *process_id_arena;

/** Cache for process structures. */
static slab_cache_t *process_cache;

/** Process containing all kernel-mode threads. */
process_t *kernel_proc;

/** Constructor for process objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int process_cache_ctor(void *obj, void *data, int kmflag) {
	process_t *process = (process_t *)obj;

	mutex_init(&process->lock, "process_lock", 0);
	refcount_set(&process->count, 0);
	list_init(&process->threads);
	notifier_init(&process->death_notifier, process);
	return 0;
}

/** Allocate a process structure and initialise it.
 * @param name		Name to give the process.
 * @param id		ID for the process (if negative, one will be allocated).
 * @param flags		Behaviour flags for the process.
 * @param cflags	Creation flags for the process.
 * @param priority	Priority to give the process.
 * @param aspace	Address space for the process.
 * @param parent	Process to inherit information from.
 * @param handles	Array of handle mappings (see handle_table_init()).
 * @param count		Number of handles in mapping array.
 * @param procp		Where to store pointer to structure.
 * @return		0 on success, negative error code on failure. */
static int process_alloc(const char *name, process_id_t id, int flags, int cflags,
                         int priority, vm_aspace_t *aspace, process_t *parent,
                         handle_id_t handles[][2], int count, process_t **procp) {
	process_t *process;
	int ret;

	assert(name);
	assert(procp);
	assert(priority >= 0 && priority < PRIORITY_MAX);

	process = slab_cache_alloc(process_cache, MM_SLEEP);

	/* Creation of the handle table is the only step that can fail. */
	if((ret = handle_table_create((parent) ? parent->handles : NULL, handles,
	                              count, &process->handles)) != 0) {
		slab_cache_free(process_cache, process);
		return ret;
	}

	object_init(&process->obj, &process_object_type);
	io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	process->id = (id < 0) ? (process_id_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : id;
	process->name = kstrdup(name, MM_SLEEP);
	process->flags = flags;
	process->priority = priority;
	process->state = PROCESS_RUNNING;
	process->aspace = aspace;

	/* Add to the process tree. */
	rwlock_write_lock(&process_tree_lock);
	avl_tree_insert(&process_tree, (key_t)process->id, process, NULL);
	rwlock_unlock(&process_tree_lock);

	dprintf("process: created process %" PRId32 "(%s) (proc: %p)\n",
		process->id, process->name, process);
	*procp = process;
	return 0;
}

/** Destroy a process structure.
 * @param process	Process to destroy. */
static void process_destroy(process_t *process) {
	assert(refcount_get(&process->count) == 0);
	assert(process->state == PROCESS_DEAD);
	assert(list_empty(&process->threads));

	rwlock_write_lock(&process_tree_lock);
	avl_tree_remove(&process_tree, (key_t)process->id);
	rwlock_unlock(&process_tree_lock);

	dprintf("process: destroyed process %" PRId32 "(%s) (process: %p, status: %d)\n",
		process->id, process->name, process, process->status);

	object_destroy(&process->obj);
	vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
	kfree(process->name);
	slab_cache_free(process_cache, process);
}

/** Closes a handle to a process.
 * @param handle	Handle to close. */
static void process_object_close(handle_t *handle) {
	process_t *process = (process_t *)handle->object;

	if(refcount_dec(&process->count) == 0) {
		process_destroy(process);
	}
}

/** Signal that a process is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int process_object_wait(object_wait_t *wait) {
	process_t *process = (process_t *)wait->handle->object;

	switch(wait->event) {
	case PROCESS_EVENT_DEATH:
		mutex_lock(&process->lock);
		if(process->state == PROCESS_DEAD) {
			object_wait_callback(wait);
		} else {
			notifier_register(&process->death_notifier, object_wait_notifier, wait);
		}
		mutex_unlock(&process->lock);
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
}

/** Stop waiting for a process.
 * @param wait		Wait information structure. */
static void process_object_unwait(object_wait_t *wait) {
	process_t *process = (process_t *)wait->handle->object;

	switch(wait->event) {
	case PROCESS_EVENT_DEATH:
		notifier_unregister(&process->death_notifier, object_wait_notifier, wait);
		break;
	}
}

/** Process object type operations. */
static object_type_t process_object_type = {
	.id = OBJECT_TYPE_PROCESS,
	.close = process_object_close,
	.wait = process_object_wait,
	.unwait = process_object_unwait,
};

/** Map a program into a new address space.
 * @param info		Pointer to information structure.
 * @return		0 on success, negative error code on failure. */
static int load_binary(process_create_info_t *info) {
	handle_t *handle;
	size_t size;
	int ret;

	/* Load the binary into the new address space. */
	if((ret = fs_file_open(info->path, FS_FILE_READ, &handle)) != 0) {
		return ret;
	} else if((ret = elf_binary_load(handle, info->aspace, &info->data)) != 0) {
		handle_release(handle);
		return ret;
	}
	handle_release(handle);

	/* Determine the size of the argument block. */
	size = sizeof(process_args_t) + (sizeof(char *) * 2) + strlen(info->path);
	for(info->argc = 0; info->args[info->argc]; size += (strlen(info->args[info->argc++]) + sizeof(char *)));
	for(info->envc = 0; info->env[info->envc]; size += (strlen(info->env[info->envc++]) + sizeof(char *)));
	size = ROUND_UP(size, PAGE_SIZE);

	/* Create a mapping for it. */
	if((ret = vm_map(info->aspace, 0, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE,
	                 NULL, 0, &info->arg_block)) != 0) {
		return ret;
	}

	/* Create a stack mapping. */
	return vm_map(info->aspace, 0, USTACK_SIZE,
	              VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE | VM_MAP_STACK,
	              NULL, 0, &info->stack);
}

/** Copy the data contained in a string array to the argument block.
 * @param dest		Array to store addresses copied to in.
 * @param source	Array to copy data of.
 * @param count		Number of array entries.
 * @param base		Base address to copy to.
 * @return		Total size copied. */
static size_t copy_argument_strings(char **dest, const char **source, size_t count, ptr_t base) {
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
	process_create_info_t *info = arg1;
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

	/* Copy path string, arguments and environment variables. */
	strcpy(uargs->path, info->path);
	addr += copy_argument_strings(uargs->args, info->args, info->argc, addr);
	addr += copy_argument_strings(uargs->env, info->env, info->envc, addr);

	/* Place the argument block address on the stack (TODO: Stack direction). */
	stack = info->stack + (USTACK_SIZE - STACK_DELTA);
	*(ptr_t *)stack = info->arg_block;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(info->data);

	/* Wake up the caller. */
	semaphore_up(&info->sem, 1);

	/* To userspace, and beyond! */
	dprintf("process: entering userspace in new process (entry: %p, stack: %p)\n", entry, stack);
	thread_arch_enter_userspace(entry, stack, 0);
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

	/* Move the process to the dead state if it contains no threads now,
	 * and clean up resources allocated to it. It is OK (and necessary) to
	 * perform the clean up with the lock not held, as only one thing
	 * should get to the clean up as we are the last thread. */
	if(list_empty(&process->threads)) {
		assert(process->state != PROCESS_DEAD);
		process->state = PROCESS_DEAD;

		mutex_unlock(&process->lock);

		if(process->flags & PROCESS_CRITICAL) {
			fatal("Critical process %" PRId32 "(%s) terminated", process->id, process->name);
		}

		notifier_run(&process->death_notifier, NULL, true);

		if(process->aspace) {
			vm_aspace_destroy(process->aspace);
			process->aspace = NULL;
		}
		handle_table_destroy(process->handles);
		io_context_destroy(&process->ioctx);
	} else {
		mutex_unlock(&process->lock);
	}

	thread->owner = NULL;

	/* Destroy the process if there are no handles remaining. */
	if(refcount_dec(&process->count) == 0) {
		process_destroy(process);
	}
}

/** Look up a process without taking the tree lock.
 * @note		This function should only be used within KDBG. Use
 *			process_lookup() outside of KDBG.
 * @param id		ID of the process to find.
 * @return		Pointer to process found, or NULL if not found. */
process_t *process_lookup_unsafe(process_id_t id) {
	return avl_tree_lookup(&process_tree, (key_t)id);
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

/** Execute a new process.
 *
 * Creates a new process and runs a program within it. The path to the process
 * should be the first argument specified in the argument structure. The new
 * process will inherit no handles from the parent.
 *
 * @param args		Arguments to pass to process (NULL-terminated array).
 * @param env		Environment to pass to process (NULL-terminated array).
 * @param flags		Behaviour flags for the process.
 * @param priority	Priority for the process.
 * @param parent	Parent for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char **args, const char **env, int flags, int priority,
                   process_t *parent, process_t **procp) {
	process_create_info_t info;
	process_t *process;
	thread_t *thread;
	int ret;

	if(!args || !args[0] || !env || priority < 0 || priority >= PRIORITY_MAX) {
		return -ERR_PARAM_INVAL;
	}

	semaphore_init(&info.sem, "process_create_sem", 0);
	info.path = args[0];
	info.args = args;
	info.env = env;
	info.aspace = vm_aspace_create();

	/* Map the binary into the new address space. */
	if((ret = load_binary(&info)) != 0) {
		vm_aspace_destroy(info.aspace);
		return ret;
	}

	/* Create the new process and run the process entry thread in it. */
	if((ret = process_alloc(args[0], -1, flags, 0, priority, info.aspace, parent,
	                        NULL, 0, &process)) != 0) {
		vm_aspace_destroy(info.aspace);
		return ret;
	} else if((ret = thread_create("main", process, 0, process_entry_thread,
	                               &info, NULL, &thread)) != 0) {
		process_destroy(process);
		vm_aspace_destroy(info.aspace);
		return ret;
	}
	thread_run(thread);

	/* Wait for the thread to finish using the information structure. */
	semaphore_down(&info.sem);
	if(procp) {
		*procp = process;
	}
	return 0;
}

/** Terminate the calling process.
 *
 * Terminates the calling process. All threads in the process will also be
 * terminated. The status code given can be retrieved by any processes with a
 * handle to the process.
 *
 * @param status	Exit status code.
 */
void process_exit(int status) {
	thread_t *thread;

	mutex_lock(&curr_proc->lock);

	LIST_FOREACH_SAFE(&curr_proc->threads, iter) {
		thread = list_entry(iter, thread_t, owner_link);

		if(thread != curr_thread) {
			thread_kill(thread);
		}
	}

	curr_proc->status = status;
	mutex_unlock(&curr_proc->lock);

	thread_exit();
}

/** Dump the contents of the process tree.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		Always returns KDBG_OK. */
int kdbg_cmd_process(int argc, char **argv) {
	process_t *process;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all running processes.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID     State   Prio Flags Count  Aspace             Name\n");
	kprintf(LOG_NONE, "==     =====   ==== ===== =====  ======             ====\n");

	AVL_TREE_FOREACH(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRId32 "%s ", process->id,
		        (process == curr_proc) ? "*" : " ");
		switch(process->state) {
		case PROCESS_RUNNING:	kprintf(LOG_NONE, "Running "); break;
		case PROCESS_DEAD:	kprintf(LOG_NONE, "Dead    "); break;
		default:		kprintf(LOG_NONE, "Bad     "); break;
		}
		kprintf(LOG_NONE, "%-4d %-5d %-6d %-18p %s\n", process->priority,
		        process->flags, refcount_get(&process->count),
			process->aspace, process->name);
	}

	return KDBG_OK;
}

/** Initialise the process table and slab cache. */
void __init_text process_init(void) {
	int ret;

	/* Create the process slab cache and ID vmem arena. */
	process_id_arena = vmem_create("process_id_arena", 1, 65535, 1, NULL, NULL, NULL, 0, 0, MM_FATAL);
	process_cache = slab_cache_create("process_cache", sizeof(process_t), 0,
	                                  process_cache_ctor, NULL, NULL, NULL,
	                                  SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);

	/* Create the kernel process. */
	if((ret = process_alloc("[kernel]", 0, PROCESS_CRITICAL | PROCESS_FIXEDPRIO,
	                        0, PRIORITY_KERNEL, NULL, NULL, NULL, 0,
	                        &kernel_proc)) != 0) {
		fatal("Could not initialise kernel process (%d)", ret);
	}
}

/** Helper to free information copied from userspace.
 * @note		Does not free the address space.
 * @param info		Structure containing copied information. */
static void process_create_info_free(process_create_info_t *info) {
	int i;

	if(info->path) {
		kfree((char *)info->path);
	}
	if(info->args) {
		for(i = 0; info->args[i]; i++) {
			kfree((char *)info->args[i]);
		}
		kfree(info->args);
	}
	if(info->env) {
		for(i = 0; info->env[i]; i++) {
			kfree((char *)info->env[i]);
		}
		kfree(info->env);
	}
	if(info->handles) {
		kfree(info->handles);
	}
}

/** Helper to copy process creation information from userspace.
 * @param path		Path to copy.
 * @param args		Argument array to copy.
 * @param env		Environment array to copy.
 * @param handles	Handle array to copy.
 * @param count		Size of handle array.
 * @param info		Pointer to information structure to fill in.
 * @return		0 on success, negative error code on failure. */
static int process_create_info_init(const char *path, const char *const args[],
                                    const char *const env[], handle_id_t handles[][2],
                                    int count, process_create_info_t *info) {
	size_t size;
	int ret;

	info->path = NULL;
	info->args = NULL;
	info->env = NULL;
	info->handles = NULL;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, (char **)&info->path)) != 0) {
		return ret;
	} else if((ret = arrcpy_from_user(args, (char ***)&info->args)) != 0) {
		process_create_info_free(info);
		return ret;
	} else if((ret = arrcpy_from_user(env, (char ***)&info->env)) != 0) {
		process_create_info_free(info);
		return ret;
	} else if(count > 0) {
		size = sizeof(handle_id_t) * 2 * count;
		if(!(info->handles = kmalloc(size, 0))) {
			process_create_info_free(info);
			return -ERR_NO_MEMORY;
		} else if((ret = memcpy_from_user(info->handles, handles, size)) != 0) {
			process_create_info_free(info);
			return ret;
		}
	}

	semaphore_init(&info->sem, "process_create_sem", 0);
	info->aspace = vm_aspace_create();
	return 0;
}

/** Create a new process.
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
 * @param handles	Array containing handle mappings (can be NULL if count
 *			is less than or equal to 0). The first ID of each entry
 *			specifies the handle in the caller, and the second
 *			specifies the ID to give it in the child.
 * @param count		Number of entries in handle mapping array.
 *
 * @return		Handle to new process (greater than or equal to 0) on
 *			success, negative error code on failure.
 */
handle_id_t sys_process_create(const char *path, const char *const args[],
                               const char *const env[], int flags,
                               handle_id_t handles[][2], int count) {
	process_create_info_t info;
	process_t *process = NULL;
	thread_t *thread = NULL;
	handle_id_t hid = -1;
	handle_t *handle;
	int ret;

	if((ret = process_create_info_init(path, args, env, handles, count, &info)) != 0) {
		return ret;
	}

	/* Map the binary into the new address space. */
	if((ret = load_binary(&info)) != 0) {
		goto fail;
	}

	/* Create the new process and then create a handle to it. The handle
	 * must be created before the process begins running, as it could fail
	 * and leave the process running, but make the caller think it isn't
	 * running. */
	if((ret = process_alloc(info.path, -1, 0, flags, PRIORITY_USER, info.aspace,
	                        curr_proc, info.handles, count, &process)) != 0) {
		goto fail;
	}
	refcount_inc(&process->count);
	handle = handle_create(&process->obj, NULL);
	hid = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	if(hid < 0) {
		ret = hid;
		goto fail;
	}

	/* Create the entry thread to finish loading the program. */
	if((ret = thread_create("main", process, 0, process_entry_thread, &info, NULL, &thread)) != 0) {
		goto fail;
	}
	thread_run(thread);

	/* Wait for the thread to finish using the information structure. */
	semaphore_down(&info.sem);
	process_create_info_free(&info);
	return hid;
fail:
	/* If the process was created but failure occurred during handle
	 * creation, This will handle process destruction. */
	if(hid >= 0) {
		handle_detach(curr_proc, hid);
	}
	vm_aspace_destroy(info.aspace);
	process_create_info_free(&info);
	return ret;
}

/** Replace the current process.
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
 * @param handles	Array containing handle mappings (can be NULL if count
 *			is less than or equal to 0). The first ID of each entry
 *			specifies the handle in the caller, and the second
 *			specifies the ID to move it to.
 * @param count		Number of entries in handle mapping array.
 *
 * @return		Does not return on success, returns negative error code
 *			on failure.
 */
int sys_process_replace(const char *path, const char *const args[], const char *const env[],
                        handle_id_t handles[][2], int count) {
	handle_table_t *table, *oldtable;
	process_create_info_t info;
	thread_t *thread = NULL;
	vm_aspace_t *oldas;
	char *oldname;
	int ret;

	if(curr_proc->threads.next->next != &curr_proc->threads) {
		kprintf(LOG_WARN, "TODO: Terminate other threads\n");
		return -ERR_NOT_IMPLEMENTED;
	}

	if((ret = process_create_info_init(path, args, env, handles, count, &info)) != 0) {
		return ret;
	}

	/* Map the binary into the new address space. */
	if((ret = load_binary(&info)) != 0) {
		goto fail;
	}

	/* Create the entry thread to finish loading the program. */
	if((ret = thread_create("main", curr_proc, 0, process_entry_thread, &info, NULL, &thread)) != 0) {
		goto fail;
	}

	/* Create a new handle table for the process. */
	if((ret = handle_table_create(curr_proc->handles, info.handles, count, &table)) != 0) {
		goto fail;
	}

	/* Switch over to the new address space and handle table. */
	mutex_lock(&curr_proc->lock);
	sched_preempt_disable();
	vm_aspace_switch(info.aspace);
	oldas = curr_proc->aspace;
	curr_proc->aspace = info.aspace;
	sched_preempt_enable();
	oldtable = curr_proc->handles;
	curr_proc->handles = table;
	oldname = curr_proc->name;
	curr_proc->name = kstrdup(info.path, MM_SLEEP);
	mutex_unlock(&curr_proc->lock);

	/* Free up old data. */
	vm_aspace_destroy(oldas);
	handle_table_destroy(oldtable);
	kfree(oldname);

	/* Run the thread and wait for it to complete, then free up data and
	 * exit this thread. */
	thread_run(thread);
	semaphore_down(&info.sem);
	process_create_info_free(&info);
	thread_exit();
fail:
	if(thread) {
		thread_destroy(thread);
	}
	vm_aspace_destroy(info.aspace);
	process_create_info_free(&info);
	return ret;
}

/** Create a duplicate of the calling process.
 *
 * Creates a new process that is a duplicate of the calling process. Any shared
 * memory regions in the parent's address space are shared between the parent
 * and the child. Private regions are made copy-on-write in the parent and the
 * child, so they both share the pages in the regions until either of them
 * write to them. Note that use of this function is not allowed if the calling
 * process has more than 1 thread.
 *
 * @param handlep	On success, a handle to the created process will be
 *			stored here in the parent.
 *
 * @return		Upon success, 0 will be returned in the parent (with
 *			a handle to the process stored in handlep), and 1 will
 *			be returned in the child. Upon failure no child process
 *			will be created and a negative error code will be
 *			returned.
 */
int sys_process_clone(handle_id_t *handlep) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Open a handle to a process.
 * @param id		Global ID of the process to open. */
handle_id_t sys_process_open(process_id_t id) {
	process_t *process;
	handle_t *handle;
	handle_id_t ret;

	rwlock_read_lock(&process_tree_lock);

	if(!(process = avl_tree_lookup(&process_tree, (key_t)id))) {
		rwlock_unlock(&process_tree_lock);
		return -ERR_NOT_FOUND;
	} else if(process->state == PROCESS_DEAD || process == kernel_proc) {
		rwlock_unlock(&process_tree_lock);
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&process->count);
	rwlock_unlock(&process_tree_lock);

	handle = handle_create(&process->obj, NULL);
	ret = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	return ret;
}

/** Get the ID of a process.
 *
 * Gets the ID of the process referred to by a handle. If the handle is
 * specified as -1, then the ID of the calling process will be returned.
 *
 * @param handle	Handle for process to get ID of.
 *
 * @return		Process ID on success (greater than or equal to zero),
 *			negative error code on failure.
 */
process_id_t sys_process_id(handle_id_t handle) {
	process_t *process;
	process_id_t id;
	handle_t *obj;

	if(handle == -1) {
		id = curr_proc->id;
	} else if(!(id = handle_lookup(curr_proc, handle, OBJECT_TYPE_PROCESS, &obj))) {
		process = (process_t *)obj->object;
		id = process->id;
		handle_release(obj);
	}

	return id;
}

/** Query the exit status of a process.
 * @param handle	Handle to process.
 * @param statusp	Where to store exit status of process.
 * @return		0 on success, negative error code on failure. */
int sys_process_status(handle_id_t handle, int *statusp) {
	process_t *process;
	handle_t *obj;
	int ret;

	if((ret = handle_lookup(curr_proc, handle, OBJECT_TYPE_PROCESS, &obj)) != 0) {
		return ret;
	}

	process = (process_t *)obj->object;
	if(process->state != PROCESS_DEAD) {
		handle_release(obj);
		return -ERR_PROCESS_RUNNING;
	}

	ret = memcpy_to_user(statusp, &process->status, sizeof(int));
	handle_release(obj);
	return ret;
}

/** Terminate the calling process.
 *
 * Terminates the calling process. All threads in the process will also be
 * terminated. The status code given can be retrieved by any processes with a
 * handle to the process.
 *
 * @param status	Exit status code.
 */
void sys_process_exit(int status) {
	process_exit(status);
}
