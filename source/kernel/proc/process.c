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
#include <object.h>
#include <vmem.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Structure to pass information into the main thread of a new process. */
typedef struct process_create_info {
	const char *path;	/**< Path to program. */
	const char **args;	/**< Argument array. */
	const char **environ;	/**< Environment array. */
	semaphore_t sem;	/**< Semaphore to wake upon completion. */
	int ret;		/**< Return code. */
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
	return 0;
}

/** Allocate a process structure and initialise it.
 * @param name		Name to give the process.
 * @param id		ID for the process (if negative, one will be allocated).
 * @param flags		Behaviour flags for the process.
 * @param cflags	Creation flags for the process.
 * @param aspace	Whether to give the structure an address space.
 * @param priority	Priority to give the process.
 * @param parent	Process to inherit information from.
 * @param procp		Where to store pointer to structure.
 * @return		0 on success, negative error code on failure. */
static int process_alloc(const char *name, process_id_t id, int flags, int cflags,
                         bool aspace, int priority, process_t *parent,
                         process_t **procp) {
	process_t *process;

	assert(name);
	assert(procp);
	assert(priority >= 0 && priority < PRIORITY_MAX);

	process = slab_cache_alloc(process_cache, MM_SLEEP);
	object_init(&process->obj, &process_object_type);
	if(parent && cflags & PROCESS_CREATE_INHERIT) {
		handle_table_init(&process->handles, &parent->handles);
	} else {
		handle_table_init(&process->handles, NULL);
	}
	io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	notifier_init(&process->death_notifier, process);
	process->id = (id < 0) ? (process_id_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : id;
	process->name = kstrdup(name, MM_SLEEP);
	process->flags = flags;
	process->priority = priority;
	process->state = PROCESS_RUNNING;
	process->aspace = (aspace) ? vm_aspace_create() : NULL;

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

/** Create the argument block for the current process.
 * @param kpath		Path string.
 * @param kargs		Argument array.
 * @param kenv		Environment array.
 * @param addrp		Where to store address of argument block.
 * @return		0 on success, negative error code on failure. */
static int copy_arguments(const char *kpath, const char **kargs, const char **kenv, ptr_t *addrp) {
	process_args_t *uargs;
	int argc, envc;
	size_t size;
	ptr_t addr;
	int ret;

	/* Get the number of entries and the total size required. */
	size = sizeof(process_args_t) + (sizeof(char *) * 2);
	for(argc = 0; kargs[argc]; size += (strlen(kargs[argc++]) + sizeof(char *)));
	for(envc = 0; kenv[envc]; size += (strlen(kenv[envc++]) + sizeof(char *)));
	size = ROUND_UP(size, PAGE_SIZE);

	/* Allocate a chunk of memory for the data. */
	if((ret = vm_map(curr_aspace, 0, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE,
	                 NULL, 0, &addr)) != 0) {
		return ret;
	}
	*addrp = addr;

	/* Fill out the structure with addresses for the arrays. */
	uargs = (process_args_t *)addr;
	addr += sizeof(process_args_t);
	uargs->path = (char *)addr;
	addr += strlen(kpath) + 1;
	uargs->args = (char **)addr;
	addr += (argc + 1) * sizeof(char *);
	uargs->env = (char **)addr;
	addr += (envc + 1) * sizeof(char *);
	uargs->args_count = argc;
	uargs->env_count = envc;

	/* Copy path string. */
	strcpy(uargs->path, kpath);

	/* Copy actual data for the arrays. */
	addr += copy_argument_strings(uargs->args, kargs, argc, addr);
	addr += copy_argument_strings(uargs->env, kenv, envc, addr);
	return 0;
}

/** Main thread for creating a new process.
 * @param arg1		Pointer to creation information structure.
 * @param arg2		Unused. */
static void process_create_thread(void *arg1, void *arg2) {
	process_create_info_t *info = arg1;
	ptr_t stack, entry, uargs;
	handle_t *handle = NULL;
	void *data = NULL;
	int ret;

	assert(info->path);
	assert(info->args);
	assert(info->environ);
	assert(curr_proc->aspace == curr_aspace);

	/* Open a handle to the binary. */
	if((ret = fs_file_open(info->path, FS_FILE_READ, &handle)) != 0) {
		goto fail;
	}

	/* Get the ELF loader to do the main work of loading the binary. */
	if((ret = elf_binary_load(handle, curr_aspace, &data)) != 0) {
		goto fail;
	}

	/* Copy arguments to the process' address space. */
	if((ret = copy_arguments(info->path, info->args, info->environ, &uargs)) != 0) {
		goto fail;
	}

	/* Create a userspace stack and place the argument block address on it.
	 * TODO: Stack direction! */
	if((ret = vm_map(curr_aspace, 0, USTACK_SIZE,
	                 VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE | VM_MAP_STACK,
	                 NULL, 0, &stack)) != 0) {
		goto fail;
	}
	stack += (USTACK_SIZE - STACK_DELTA);
	*(unative_t *)stack = uargs;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(data);

	/* Clean up our mess and wake up the caller. */
	elf_binary_cleanup(data);
	handle_release(handle);
	semaphore_up(&info->sem, 1);

	/* To userspace, and beyond! */
	dprintf("process: entering userspace in new process (entry: %p, stack: %p)\n", entry, stack);
	thread_arch_enter_userspace(entry, stack, 0);
	fatal("Failed to enter userspace!");
fail:
	if(data) {
		elf_binary_cleanup(data);
	}
	if(handle) {
		handle_release(handle);
	}
	info->ret = ret;
	semaphore_up(&info->sem, 1);
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
		handle_table_destroy(&process->handles);
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
 * should be the first argument specified in the argument structure.
 *
 * @param args		Arguments to pass to process (NULL-terminated array).
 * @param environ	Environment to pass to process (NULL-terminated array).
 * @param flags		Behaviour flags for the process.
 * @param cflags	Creation flags for the process.
 * @param priority	Priority for the process.
 * @param parent	Parent for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char **args, const char **environ, int flags, int cflags,
                   int priority, process_t *parent, process_t **procp) {
	process_create_info_t info;
	process_t *process;
	thread_t *thread;
	int ret;

	if(!args || !args[0] || !environ || priority < 0 || priority >= PRIORITY_MAX) {
		return -ERR_PARAM_INVAL;
	}

	/* Fill in the information structure to pass information into the
	 * main thread of the new process. */
	semaphore_init(&info.sem, "process_create_sem", 0);
	info.path = args[0];
	info.args = args;
	info.environ = environ;
	info.ret = 0;

	if((ret = process_alloc(args[0], -1, flags, cflags, true, priority, parent, &process)) != 0) {
		return ret;
	} else if((ret = thread_create("main", process, 0, process_create_thread, &info, NULL, &thread)) != 0) {
		process_destroy(process);
		return ret;
	}
	thread_run(thread);

	/* Wait for completion, and return. No cleanup is necessary as the
	 * process/thread will be cleaned up by the normal mechanism. */
	semaphore_down(&info.sem);
	if(info.ret == 0 && procp) {
		*procp = process;
	}
	return info.ret;
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
	                        0, false, PRIORITY_KERNEL, NULL, &kernel_proc)) != 0) {
		fatal("Could not initialise kernel process (%d)", ret);
	}
}

/** Helper to copy information from userspace.
 * @param path		Path to copy.
 * @param args		Argument array to copy.
 * @param environ	Environment array to copy.
 * @param kpathp	Where to store kernel address of path.
 * @param kargsp	Where to store kernel address of argument array.
 * @param kenvp		Where to store kernel address of environment array.
 * @return		0 on success, negative error code on failure. */
static int sys_process_arg_copy(const char *path, const char *const args[], const char *const environ[],
                                const char **kpathp, const char ***kargsp, const char ***kenvp) {
	char **kargs, **kenv, *kpath;
	int ret;

	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return ret;
	} else if((ret = arrcpy_from_user(args, &kargs)) != 0) {
		kfree(kpath);
		return ret;
	} else if((ret = arrcpy_from_user(environ, &kenv)) != 0) {
		for(int i = 0; kargs[i] != NULL; i++) {
			kfree(kargs[i]);
		}

		kfree(kargs);
		kfree(kpath);
		return ret;
	}

	*kpathp = (const char *)kpath;
	*kargsp = (const char **)kargs;
	*kenvp = (const char **)kenv;
	return 0;
}

/** Helper to free information copied from userspace.
 * @param path		Path to free.
 * @param args		Argument array to free.
 * @param environ	Environment array to free. */
static void sys_process_arg_free(const char *path, const char **args, const char **environ) {
	for(int i = 0; args[i] != NULL; i++) {
		kfree((char *)args[i]);
	}
	for(int i = 0; environ[i] != NULL; i++) {
		kfree((char *)environ[i]);
	}
	kfree((char *)path);
	kfree(args);
	kfree(environ);
}

/** Create a new process.
 *
 * Creates a new process and executes a program within it. If specified,
 * handles marked as inheritable in the calling process will be inherited by
 * the new process (with the same IDs).
 *
 * @todo		Inheriting.
 *
 * @param path		Path to program to execute.
 * @param args		Argument array (with NULL terminator).
 * @param environ	Environment array (with NULL terminator).
 * @param flags		Flags specifying creation behaviour.
 *
 * @return		Handle to new process (greater than or equal to 0),
 *			negative error code on failure.
 */
handle_id_t sys_process_create(const char *path, const char *const args[], const char *const environ[], int flags) {
	process_create_info_t info;
	process_t *process = NULL;
	thread_t *thread = NULL;
	handle_id_t hid = -1;
	handle_t *handle;
	int ret;

	if((ret = sys_process_arg_copy(path, args, environ, &info.path, &info.args, &info.environ)) != 0) {
		return ret;
	}

	/* Create a structure for the process. */
	if((ret = process_alloc(info.path, -1, 0, flags, true, PRIORITY_USER, curr_proc, &process)) != 0) {
		goto fail;
	}

	/* Try to create the handle for the process. This should not be left
	 * until after the process has begun running, because it could fail and
	 * leave the new process running, but make the caller think it isn't
	 * running. */
	refcount_inc(&process->count);
	handle = handle_create(&process->obj, NULL);
	hid = handle_attach(curr_proc, handle, 0);
	handle_release(handle);
	if(hid < 0) {
		ret = (int)hid;
		goto fail;
	}

	/* Fill in the information structure to pass information into the main
	 * thread of the new process. */
	semaphore_init(&info.sem, "process_create_sem", 0);
	info.ret = 0;

	if((ret = thread_create("main", process, 0, process_create_thread, &info, NULL, &thread)) != 0) {
		goto fail;
	}
	thread_run(thread);

	/* Wait for completion and check the return code. */
	semaphore_down(&info.sem);
	if((ret = info.ret) != 0) {
		goto fail;
	}

	sys_process_arg_free(info.path, info.args, info.environ);
	return hid;
fail:
	if(hid >= 0) {
		/* This will handle process destruction. */
		handle_detach(curr_proc, hid);
	}
	sys_process_arg_free(info.path, info.args, info.environ);
	return (handle_id_t)ret;
}

/** Replace the current process.
 *
 * Replaces the current process with a new program. All threads in the process
 * other than the calling thread will be terminated. If specified, handles
 * marked as inheritable in the process will remain open in the new process,
 * and others will be closed. Otherwise, all handles will be closed.
 *
 * @todo		Inheriting.
 * @todo		Bit of code duplication here between process_create().
 *
 * @param path		Path to program to execute.
 * @param args		Argument array (with NULL terminator).
 * @param environ	Environment array (with NULL terminator).
 * @param flags		Flags specifying creation behaviour.
 *
 * @return		Does not return on success, returns negative error code
 *			on failure.
 */
int sys_process_replace(const char *path, const char *const args[], const char *const environ[], int flags) {
	const char **kargs, **kenv, *kpath;
	vm_aspace_t *as = NULL, *old;
	ptr_t stack, entry, uargs;
	handle_t *handle = NULL;
	void *data = NULL;
	char *name;
	int ret;

	if((ret = sys_process_arg_copy(path, args, environ, &kpath, &kargs, &kenv)) != 0) {
		return ret;
	} else if(!kargs[0]) {
		ret = -ERR_PARAM_INVAL;
		goto fail;
	} else if(curr_proc->threads.next->next != &curr_proc->threads) {
		kprintf(LOG_WARN, "TODO: Terminate other threads\n");
		ret = -ERR_NOT_IMPLEMENTED;
		goto fail;
	}

	/* Open a handle to the binary. */
	if((ret = fs_file_open(kpath, FS_FILE_READ, &handle)) != 0) {
		goto fail;
	}

	/* Create a new address space to load the binary into. */
	as = vm_aspace_create();

	/* Get the ELF loader to do the main work of loading the binary. */
	if((ret = elf_binary_load(handle, as, &data)) != 0) {
		goto fail;
	}

	/* Set the new name and address space. */
	mutex_lock(&curr_proc->lock);
	name = curr_proc->name;
	curr_proc->name = kstrdup(kpath, MM_SLEEP);
	old = curr_proc->aspace;
	curr_proc->aspace = as;
	vm_aspace_switch(as);
	mutex_unlock(&curr_proc->lock);

	/* Now that the lock is no longer held, free up old data. */
	kfree(name);
	vm_aspace_destroy(old);

	/* Copy arguments to the process' address space. TODO: Better failure
	 * handling here. */
	if((ret = copy_arguments(kpath, kargs, kenv, &uargs)) != 0) {
		fatal("Meep, need to handle this (%d)", ret);
	}

	/* Create a userspace stack and place the argument block address on it.
	 * TODO: Stack direction! */
	if((ret = vm_map(curr_aspace, 0, USTACK_SIZE,
	                 VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE | VM_MAP_STACK,
	                 NULL, 0, &stack)) != 0) {
		fatal("Meep, need to handle this too (%d)", ret);
	}
	stack += (USTACK_SIZE - STACK_DELTA);
	*(unative_t *)stack = uargs;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(data);

	/* Clean up our mess. */
	elf_binary_cleanup(data);
	handle_release(handle);
	sys_process_arg_free(kpath, kargs, kenv);

	/* To userspace, and beyond! */
	dprintf("process: entering userspace in process %" PRId32 " (entry: %p, stack: %p)\n", curr_proc->id, entry, stack);
	thread_arch_enter_userspace(entry, stack, 0);
	fatal("Failed to enter userspace!");
fail:
	if(data) {
		elf_binary_cleanup(data);
	}
	if(as) {
		vm_aspace_destroy(as);
	}
	if(handle) {
		handle_release(handle);
	}
	sys_process_arg_free(kpath, kargs, kenv);
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
int sys_process_duplicate(handle_id_t *handlep) {
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
