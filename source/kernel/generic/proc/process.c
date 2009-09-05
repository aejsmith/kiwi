/* Kiwi process management
 * Copyright (C) 2008-2009 Alex Smith
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
 */

#include <console/kprintf.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>
#include <mm/vmem.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <types/avl.h>

#include <assert.h>
#include <elf.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>

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

extern void process_destroy(process_t *process);

/** Process containing all kernel-mode threads. */
process_t *kernel_proc;

static AVL_TREE_DECLARE(process_tree);		/**< Tree of all processes. */
static MUTEX_DECLARE(process_tree_lock, 0);	/**< Lock for process AVL tree. */
static vmem_t *process_id_arena;		/**< Process ID Vmem arena. */
static slab_cache_t *process_cache;		/**< Cache for process structures. */

/** Constructor for process objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int process_cache_ctor(void *obj, void *data, int kmflag) {
	process_t *process = (process_t *)obj;

	spinlock_init(&process->lock, "process_lock");
	refcount_set(&process->count, 0);
	list_init(&process->threads);
	return 0;
}

/** Allocate a process structure and initialize it.
 * @param name		Name to give the process.
 * @param id		ID for the process (if negative, one will be allocated).
 * @param flags		Flags for the process.
 * @param priority	Priority to give the process.
 * @param parent	Process to inherit information from.
 * @param aspace	Whether to give the structure an address space.
 * @param inherit	Whether to inherit handles from the parent.
 * @param procp		Where to store pointer to structure.
 * @return		0 on success, negative error code on failure. */
static int process_alloc(const char *name, identifier_t id, int flags, int priority,
                         process_t *parent, bool aspace, bool inherit,
                         process_t **procp) {
	process_t *process = slab_cache_alloc(process_cache, MM_SLEEP);
	int ret;

	assert(name);
	assert(procp);
	assert(priority >= 0 && priority < PRIORITY_MAX);

	/* Create the address space. */
	if(aspace) {
		if(!(process->aspace = vm_aspace_create())) {
			slab_cache_free(process_cache, process);
			return -ERR_NO_MEMORY;
		}
	} else {
		process->aspace = NULL;
	}

	/* Initialize the process' handle table. */
	if((ret = handle_table_init(&process->handles, (parent && inherit) ? &parent->handles : NULL)) != 0) {
		if(process->aspace) {
			vm_aspace_destroy(process->aspace);
		}
		slab_cache_free(process_cache, process);
		return ret;
	}

	/* Initialize other information for the process. Do this after all the
	 * steps that can fail to make life easier when handling failure. */
	io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	notifier_init(&process->death_notifier, process);
	process->id = (id < 0) ? (identifier_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : id;
	process->name = kstrdup(name, MM_SLEEP);
	process->flags = flags;
	process->priority = priority;

	/* Add to the process tree. */
	mutex_lock(&process_tree_lock, 0);
	avl_tree_insert(&process_tree, (key_t)process->id, process, NULL);
	mutex_unlock(&process_tree_lock);

	*procp = process;

	dprintf("process: created process %" PRId32 "(%s) (proc: %p)\n",
		process->id, process->name, process);
	return 0;
}

/** Copy the data contained in a string array to the argument block.
 * @param dest		Array to store addresses copied to in.
 * @param source	Array to copy data of.
 * @param count		Number of array entries.
 * @param base		Base address to copy to.
 * @return		Total size copied. */
static size_t process_copy_args_data(char **dest, const char **source, size_t count, ptr_t base) {
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
static int process_copy_args(const char *kpath, const char **kargs, const char **kenv, ptr_t *addrp) {
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
	if((ret = vm_map_anon(curr_aspace, 0, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, &addr)) != 0) {
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
	addr += process_copy_args_data(uargs->args, kargs, argc, addr);
	addr += process_copy_args_data(uargs->env, kenv, envc, addr);
	return 0;
}

/** Main thread for creating a new process.
 * @param arg1		Pointer to creation information structure.
 * @param arg2		Unused. */
static void process_create_thread(void *arg1, void *arg2) {
	process_create_info_t *info = arg1;
	ptr_t stack, entry, uargs;
	vfs_node_t *node = NULL;
	void *data = NULL;
	int ret;

	assert(info->path);
	assert(info->args);
	assert(info->environ);
	assert(curr_proc->aspace == curr_aspace);

	/* Look up the node on the filesystem. */
	if((ret = vfs_node_lookup(info->path, true, VFS_NODE_FILE, &node)) != 0) {
		goto fail;
	}

	/* Get the ELF loader to do the main work of loading the binary. */
	if((ret = elf_binary_load(node, curr_aspace, &data)) != 0) {
		goto fail;
	}

	/* Copy arguments to the process' address space. */
	if((ret = process_copy_args(info->path, info->args, info->environ, &uargs)) != 0) {
		goto fail;
	}

	/* Create a userspace stack and place the argument block address on it.
	 * TODO: Stack direction! */
	if((ret = vm_map_anon(curr_aspace, 0, USTACK_SIZE, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, &stack)) != 0) {
		goto fail;
	}
	stack += (USTACK_SIZE - STACK_DELTA);
	*(unative_t *)stack = uargs;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(data);

	/* Clean up our mess and wake up the caller. */
	elf_binary_cleanup(data);
	vfs_node_release(node);
	semaphore_up(&info->sem, 1);

	/* To userspace, and beyond! */
	dprintf("process: entering userspace in new process (entry: %p, stack: %p)\n", entry, stack);
	thread_arch_enter_userspace(entry, stack, 0);
	fatal("Failed to enter userspace!");
fail:
	if(data) {
		elf_binary_cleanup(data);
	}
	if(node) {
		vfs_node_release(node);
	}
	info->ret = ret;
	semaphore_up(&info->sem, 1);
}

/** Execute a new process.
 *
 * Creates a new process and runs a program within it. The path to the process
 * should be the first argument specified in the argument structure.
 *
 * @param args		Arguments to pass to process (NULL-terminated array).
 * @param environ	Environment to pass to process (NULL-terminated array).
 * @param flags		Behaviour flags for the process.
 * @param priority	Priority for the process.
 * @param parent	Parent for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char **args, const char **environ, int flags, int priority,
                   process_t *parent, process_t **procp) {
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

	if((ret = process_alloc(args[0], -1, flags, priority, parent, true, false, &process)) != 0) {
		return ret;
	} else if((ret = thread_create("main", process, 0, process_create_thread, &info, NULL, &thread)) != 0) {
		process_destroy(process);
		return ret;
	}
	thread_run(thread);

	/* Wait for completion, and return. No cleanup is necessary as the
	 * process/thread will be cleaned up by the normal mechanism. */
	semaphore_down(&info.sem, 0);
	if(info.ret == 0 && procp) {
		*procp = process;
	}
	return info.ret;
}

/** Lookup a process.
 *
 * Looks for a process with the specified ID in the process tree.
 *
 * @param id		ID of the process to find.
 *
 * @return		Pointer to process found, or NULL if not found.
 */
process_t *process_lookup(identifier_t id) {
	process_t *process;

	/* Small hack so that KDBG functions can use this. */
	if(atomic_get(&kdbg_running)) {
		process = avl_tree_lookup(&process_tree, (key_t)id);
	} else {
		mutex_lock(&process_tree_lock, 0);
		process = avl_tree_lookup(&process_tree, (key_t)id);
		mutex_unlock(&process_tree_lock);
	}

	return process;
}

/** Destroy a process.
 *
 * Destroys a process structure. The reference count of the process must be 0.
 * This should only be called from the thread destruction code and from the
 * process handle management code.
 *
 * @param process	Process to destroy.
 */
void process_destroy(process_t *process) {
	assert(refcount_get(&process->count) == 0);
	assert(list_empty(&process->threads));

	if(process->flags & PROCESS_CRITICAL) {
		fatal("Critical process %" PRId32 "(%s) terminated", process->id, process->name);
	}

	mutex_lock(&process_tree_lock, 0);
	avl_tree_remove(&process_tree, (key_t)process->id);
	mutex_unlock(&process_tree_lock);

	/* Run and destroy the death notifier list. */
	notifier_run(&process->death_notifier, NULL);
	notifier_destroy(&process->death_notifier);

	if(process->aspace) {
		vm_aspace_destroy(process->aspace);
	}
	handle_table_destroy(&process->handles);
	io_context_destroy(&process->ioctx);

	dprintf("process: destroyed process %" PRId32 "(%s) (process: %p, status: %d)\n",
		process->id, process->name, process, process->status);

	vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
	kfree(process->name);
	slab_cache_free(process_cache, process);
}

/** Initialize the process table and slab cache. */
void __init_text process_init(void) {
	int ret;

	/* Create the process slab cache and ID vmem arena. */
	process_id_arena = vmem_create("process_id_arena", 1, 65534, 1, NULL, NULL, NULL, 0, MM_FATAL);
	process_cache = slab_cache_create("process_cache", sizeof(process_t), 0,
	                                  process_cache_ctor, NULL, NULL, NULL, NULL,
	                                  0, MM_FATAL);

	/* Create the kernel process. */
	if((ret = process_alloc("[kernel]", 0, PROCESS_CRITICAL | PROCESS_FIXEDPRIO,
	                        PRIORITY_KERNEL, NULL, false, false,
	                        &kernel_proc)) != 0) {
		fatal("Could not initialize kernel process (%d)", ret);
	}
}

/** Dump the contents of the process table.
 *
 * Dumps out the contents of the process hash table.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_process(int argc, char **argv) {
	process_t *process;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all running processes.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID     Prio Flags Count  Aspace             Name\n");
	kprintf(LOG_NONE, "==     ==== ===== =====  ======             ====\n");

	AVL_TREE_FOREACH(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRId32 "%s %-4d %-5d %-6d %-18p %s\n",
			process->id, (process == curr_proc) ? "*" : " ",
		        process->priority, process->flags, refcount_get(&process->count),
			process->aspace, process->name);
	}

	return KDBG_OK;
}

#if 0
# pragma mark Process handle functions.
#endif

/** Closes a handle to a process.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int process_handle_close(handle_info_t *info) {
	process_t *process = info->data;

	if(refcount_dec(&process->count) == 0) {
		process_destroy(process);
	}
	return 0;
}

/** Process handle operations. */
static handle_type_t process_handle_type = {
	.id = HANDLE_TYPE_PROCESS,
	.close = process_handle_close,
};

#if 0
# pragma mark System calls.
#endif

/** Helper to copy information from userspace.
 * @param path		Path to copy.
 * @param args		Argument array to copy.
 * @param environ	Environment array to copy.
 * @param kpathp	Where to store kernel address of path.
 * @param kargsp	Where to store kernel address of argument array.
 * @param kenvp		Where to store kernel address of environment array.
 * @return		0 on success, negative error code on failure. */
static int sys_process_arg_copy(const char *path, char *const args[], char *const environ[],
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
 * @param inherit	Whether to inherit inheritable handles.
 *
 * @return		Handle to new process (greater than or equal to 0),
 *			negative error code on failure.
 */
handle_t sys_process_create(const char *path, char *const args[], char *const environ[], bool inherit) {
	process_create_info_t info;
	process_t *process = NULL;
	thread_t *thread = NULL;
	handle_t handle = -1;
	int ret;

	if((ret = sys_process_arg_copy(path, args, environ, &info.path, &info.args, &info.environ)) != 0) {
		return ret;
	}

	/* Create a structure for the process. */
	if((ret = process_alloc(info.path, -1, 0, PRIORITY_USER, curr_proc, true, inherit, &process)) != 0) {
		goto fail;
	}

	/* Try to create the handle for the process. This should not be left
	 * until after the process has begun running, because it could fail and
	 * leave the new process running, but make the caller think it isn't
	 * running. */
	if((handle = handle_create(&curr_proc->handles, &process_handle_type, process)) < 0) {
		ret = (int)handle;
		goto fail;
	}
	refcount_inc(&process->count);

	/* Fill in the information structure to pass information into the main
	 * thread of the new process. */
	semaphore_init(&info.sem, "process_create_sem", 0);
	info.ret = 0;

	if((ret = thread_create("main", process, 0, process_create_thread, &info, NULL, &thread)) != 0) {
		goto fail;
	}
	thread_run(thread);

	/* Wait for completion and check the return code. */
	semaphore_down(&info.sem, 0);
	if((ret = info.ret) != 0) {
		goto fail;
	}

	sys_process_arg_free(info.path, info.args, info.environ);
	return handle;
fail:
	if(handle >= 0) {
		/* This will handle process destruction. */
		handle_close(&curr_proc->handles, handle);
	}
	sys_process_arg_free(info.path, info.args, info.environ);
	return (handle_t)ret;
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
 * @param inherit	Whether to inherit inheritable handles.
 *
 * @return		Does not return on success, returns negative error code
 *			on failure.
 */
int sys_process_replace(const char *path, char *const args[], char *const environ[], bool inherit) {
	const char **kargs, **kenv, *kpath;
	vm_aspace_t *as = NULL, *old;
	ptr_t stack, entry, uargs;
	vfs_node_t *node = NULL;
	void *data = NULL;
	char *dup, *name;
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

	/* Look up the node on the filesystem. */
	if((ret = vfs_node_lookup(kpath, true, VFS_NODE_FILE, &node)) != 0) {
		goto fail;
	}

	/* Create a new address space to load the binary into. */
	if(!(as = vm_aspace_create())) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	}

	/* Get the ELF loader to do the main work of loading the binary. */
	if((ret = elf_binary_load(node, as, &data)) != 0) {
		goto fail;
	}

	/* Create a duplicate of the name before taking the process' lock, as
	 * we should not use allocators while a spinlock is held. */
	dup = kstrdup(kpath, MM_SLEEP);

	/* Set the new name and address space. */
	spinlock_lock(&curr_proc->lock, 0);
	name = curr_proc->name;
	curr_proc->name = dup;
	old = curr_proc->aspace;
	curr_proc->aspace = as;
	vm_aspace_switch(as);
	spinlock_unlock(&curr_proc->lock);

	/* Now that the lock is no longer held, free up old data. */
	kfree(name);
	vm_aspace_destroy(old);

	/* Copy arguments to the process' address space. TODO: Better failure
	 * handling here. */
	if((ret = process_copy_args(kpath, kargs, kenv, &uargs)) != 0) {
		fatal("Meep, need to handle this (%d)", ret);
	}

	/* Create a userspace stack and place the argument block address on it.
	 * TODO: Stack direction! */
	if((ret = vm_map_anon(curr_aspace, 0, USTACK_SIZE, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, &stack)) != 0) {
		fatal("Meep, need to handle this too (%d)", ret);
	}
	stack += (USTACK_SIZE - STACK_DELTA);
	*(unative_t *)stack = uargs;

	/* Get the ELF loader to clear BSS and get the entry pointer. */
	entry = elf_binary_finish(data);

	/* Clean up our mess. */
	elf_binary_cleanup(data);
	vfs_node_release(node);
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
	if(node) {
		vfs_node_release(node);
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
int sys_process_duplicate(handle_t *handlep) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Open a handle to a process.
 *
 * Opens a handle to a process in order to perform other operations on it.
 *
 * @param id		Global ID of the process to open.
 */
handle_t sys_process_open(identifier_t id) {
	process_t *process;
	handle_t handle;

	mutex_lock(&process_tree_lock, 0);

	if(!(process = avl_tree_lookup(&process_tree, (key_t)id)) || list_empty(&process->threads)) {
		mutex_unlock(&process_tree_lock);
		return -ERR_NOT_FOUND;
	}

	refcount_inc(&process->count);
	mutex_unlock(&process_tree_lock);

	if((handle = handle_create(&curr_proc->handles, &process_handle_type, process)) < 0) {
		if(refcount_dec(&process->count) == 0) {
			process_destroy(process);
		}
	}

	return handle;
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
identifier_t sys_process_id(handle_t handle) {
	handle_info_t *info;
	process_t *process;
	identifier_t id;

	if(handle == -1) {
		id = curr_proc->id;
	} else if(!(id = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_PROCESS, &info))) {
		process = info->data;
		id = process->id;
		handle_release(info);
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
	if(curr_proc->threads.next->next != &curr_proc->threads) {
		fatal("TODO: Terminate other threads!");
	}

	curr_proc->status = status;
	thread_exit();
}
