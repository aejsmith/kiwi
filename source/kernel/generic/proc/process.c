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
#include <proc/loader.h>
#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <types/avl.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

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
	list_init(&process->threads);
	list_init(&process->children);
	list_init(&process->parent_link);

	return 0;
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

/** Create a new process.
 *
 * Allocates and initializes a new process.
 *
 * @param name		Name to give the process.
 * @param parent	Parent of the process.
 * @param priority	Priority for the process.
 * @param flags		Behaviour/creation flags for the process.
 * @param procp		Where to store pointer to new process.
 *
 * @return		0 on success, negative error code on failure.
 */
int process_create(const char *name, process_t *parent, int priority, int flags, process_t **procp) {
	process_t *process;
	int ret;

	if(name == NULL || (parent == NULL && kernel_proc != NULL) || procp == NULL) {
		return -ERR_PARAM_INVAL;
	} else if(priority < 0 || priority >= PRIORITY_MAX) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate a process structure from the cache and set its name. */
	process = slab_cache_alloc(process_cache, MM_SLEEP);
	process->name = kstrdup(name, MM_SLEEP);

	/* Create the address space. */
	if(!(flags & PROCESS_NOASPACE)) {
		if(!(process->aspace = vm_aspace_create())) {
			slab_cache_free(process_cache, process);
			return -ERR_NO_MEMORY;
		}
	} else {
		flags &= ~PROCESS_NOASPACE;
		process->aspace = NULL;
	}

	/* Allocate an ID for the process. */
	process->id = (kernel_proc) ? (identifier_t)vmem_alloc(process_id_arena, 1, MM_SLEEP) : 0;

	process->flags = flags;
	process->priority = priority;
	process->num_threads = 0;
	process->state = PROC_RUNNING;
	process->parent = parent;

	/* Initialize the process' handle table. */
	ret = handle_table_init(&process->handles, (parent) ? &parent->handles : NULL);
	if(ret != 0) {
		vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
		slab_cache_free(process_cache, process);
		return ret;
	}

	/** Initialize the I/O context. */
	ret = io_context_init(&process->ioctx, (parent) ? &parent->ioctx : NULL);
	if(ret != 0) {
		handle_table_destroy(&process->handles);
		vmem_free(process_id_arena, (vmem_resource_t)process->id, 1);
		slab_cache_free(process_cache, process);
		return ret;
	}

	/* Add to the parent's process list. */
	if(parent != NULL) {
		spinlock_lock(&parent->lock, 0);
		list_append(&parent->children, &process->parent_link);
		spinlock_unlock(&parent->lock);
	}

	/* Add to the process tree. */
	mutex_lock(&process_tree_lock, 0);
	avl_tree_insert(&process_tree, (key_t)process->id, process, NULL);
	mutex_unlock(&process_tree_lock);

	*procp = process;

	dprintf("proc: created process %" PRId32 "(%s) (proc: %p, parent: %p)\n",
		process->id, process->name, process, parent);
	return 0;
}

/** Reset the specified process.
 *
 * Resets the state of the specified process. If the process is the current
 * process, then all threads in the process will be terminated other than the
 * current. Otherwise, all threads will be terminated. The name of the process
 * will then be changed to the given name and its address space will be set to
 * the given address space (the old one, if any, will be destroyed). If the
 * process is the current process, and the old address space set in the process
 * is the current address space, then this function will switch to the new
 * address space.
 *
 * @todo		The functionality to kill threads is not yet
 *			implemented. This means that it cannot be used on
 *			the current process if it has threads other than the
 *			current, or on other processes that have threads.
 *
 * @param process	Process to reset. Must not be the kernel process.
 * @param name		New name to give the process (must not be zero-length).
 * @param as		Address space for the process (can be NULL).
 *
 * @return		0 on success, negative error code on failure. The only
 *			reason for the function to fail is if it is passed
 *			invalid parameters.
 */
int process_reset(process_t *process, const char *name, vm_aspace_t *as) {
	char *dup, *pname;
	vm_aspace_t *pas;

	if(!process || process == kernel_proc || !name || !name[0]) {
		return -ERR_PARAM_INVAL;
	}

	/* Create a duplicate of the name before taking the process' lock, as
	 * we should not use allocators while a spinlock is held. */
	dup = kstrdup(name, MM_SLEEP);

	spinlock_lock(&process->lock, 0);

	/* TODO: Implement these. */
	if((process == curr_proc && process->num_threads > 1) || (process != curr_proc && process->num_threads)) {
		fatal("TODO: Implement process_reset for extra threads");
	}

	/* Reset the process' name. */
	pname = process->name;
	process->name = dup;

	/* Replace the address space, and switch to the new one. If the new
	 * address space is NULL vm_aspace_switch() will switch to the kernel
	 * addres space. */
	pas = process->aspace;
	process->aspace = as;
	if(pas == curr_aspace) {
		vm_aspace_switch(as);
	}

	spinlock_unlock(&process->lock);

	/* Now that the lock is no longer held, free up old data. */
	kfree(pname);
	if(pas) {
		vm_aspace_destroy(pas);
	}

	return 0;
}

/** Initialize the process table and slab cache. */
void __init_text process_init(void) {
	int ret;

	/* Create the process slab cache and ID Vmem arena. */
	process_id_arena = vmem_create("process_id_arena", 1, 65534, 1, NULL, NULL, NULL, 0, MM_FATAL);
	process_cache = slab_cache_create("process_cache", sizeof(process_t), 0,
	                                  process_cache_ctor, NULL, NULL, NULL, NULL,
	                                  0, MM_FATAL);

	/* Create the kernel process. */
	ret = process_create("[kernel]", NULL, PRIORITY_KERNEL,
	                     PROCESS_CRITICAL | PROCESS_FIXEDPRIO | PROCESS_NOASPACE,
	                     &kernel_proc);
	if(ret != 0) {
		fatal("Could not initialize kernel process: %d", ret);
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

	kprintf(LOG_NONE, "ID     Prio Flags Threads Aspace             Name\n");
	kprintf(LOG_NONE, "==     ==== ===== ======= ======             ====\n");

	AVL_TREE_FOREACH(&process_tree, iter) {
		process = avl_tree_entry(iter, process_t);

		kprintf(LOG_NONE, "%-5" PRId32 "%s %-4d %-5d %-7zu %-18p %s\n",
			process->id, (process == curr_proc) ? "*" : " ",
		        process->priority, process->flags, process->num_threads,
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
	//process_t *process = info->data;

	/* FIXME. */
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
                                char **kpathp, char ***kargsp, char ***kenvp) {
	char *kpath = NULL, **kargs = NULL, **kenv = NULL;
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

	*kpathp = kpath;
	*kargsp = kargs;
	*kenvp = kenv;
	return 0;
}

/** Helper to free information copied from userspace.
 * @param path		Path to free.
 * @param args		Argument array to free.
 * @param environ	Environment array to free. */
static void sys_process_arg_free(char *path, char **args, char **environ) {
	for(int i = 0; args[i] != NULL; i++) {
		kfree(args[i]);
	}
	for(int i = 0; environ[i] != NULL; i++) {
		kfree(environ[i]);
	}
	kfree(path);
	kfree(args);
	kfree(environ);
}

/** Structure containing information for sys_process_create(). */
struct process_create_info {
	char *path;		/**< Path to binary. */
	char **args;		/**< Argument array. */
	char **environ;		/**< Environment array. */
	bool inherit;		/**< Whether to inherit handles. */
	semaphore_t sem;	/**< Semaphore to wake upon completion. */
	int ret;		/**< Return code. */
};

/** Main thread for sys_process_create().
 * @param arg1		Pointer to information structure.
 * @param arg2		Unused. */
static void sys_process_create_thread(void *arg1, void *arg2) {
	struct process_create_info *info = arg1;

	info->ret = loader_binary_load(info->path, info->args, info->environ, &info->sem);

	/* Must have failed. Notify caller ourselves, which will pick up the
	 * error code. */
	assert(info->ret != 0);
	semaphore_up(&info->sem);

	kprintf(LOG_WARN, "Meep, process_create failed. Process stuck\n");
}

/** Create a new process.
 *
 * Creates a new process and executes a program within it. If specified,
 * handles marked as inheritable in the calling process will be inherited by
 * the new process (with the same IDs).
 *
 * @todo		Inheriting.
 *
 * @param path		Path to executable for new process.
 * @param args		Argument array (with NULL terminator).
 * @param environ	Environment array (with NULL terminator).
 * @param inherit	Whether to inherit inheritable handles.
 *
 * @return		Handle to new process (greater than or equal to 0),
 *			negative error code on failure.
 */
handle_t sys_process_create(const char *path, char *const args[], char *const environ[], bool inherit) {
	struct process_create_info info;
	process_t *process = NULL;
	thread_t *thread = NULL;
	handle_t handle = -1;
	int ret;

	if((ret = sys_process_arg_copy(path, args, environ, &info.path, &info.args, &info.environ)) != 0) {
		return ret;
	} else if(!info.path[0]) {
		ret = -ERR_PARAM_INVAL;
		goto fail;
	}

	/* Create the new process structure. */
	if((ret = process_create("[creating...]", curr_proc, PRIORITY_USER, PROCESS_NOASPACE, &process)) != 0) {
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

	/* Fill in the information structure to pass to the main thread. */
	semaphore_init(&info.sem, "process_create_sem", 0);
	info.inherit = inherit;
	info.ret = 0;

	/* Create the main thread for the process. */
	if((ret = thread_create("main", process, 0, sys_process_create_thread, &info, NULL, &thread)) != 0) {
		goto fail;
	}
	thread_run(thread);

	/* Wait for completion, and then check the return code. */
	semaphore_down(&info.sem, 0);
	if((ret = info.ret) != 0) {
		goto fail;
	}

	return handle;
fail:
	if(process || thread) {
		kprintf(LOG_WARN, "FIXME: Destroy process/thread on failure\n");
	}
	if(handle >= 0) {
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
 *
 * @param path		Path to executable for new process.
 * @param args		Argument array (with NULL terminator).
 * @param environ	Environment array (with NULL terminator).
 * @param inherit	Whether to inherit inheritable handles.
 *
 * @return		Does not return on success, returns negative error code
 *			on failure.
 */
int sys_process_replace(const char *path, char *const args[], char *const environ[], bool inherit) {
	char *kpath, **kargs, **kenv;
	int ret;

	if((ret = sys_process_arg_copy(path, args, environ, &kpath, &kargs, &kenv)) != 0) {
		return ret;
	} else if(!kpath[0]) {
		sys_process_arg_free(kpath, kargs, kenv);
		return -ERR_PARAM_INVAL;
	}

	/* If this returns it has failed. */
	ret = loader_binary_load(kpath, kargs, kenv, NULL);
	assert(ret != 0);
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
	return -ERR_NOT_IMPLEMENTED;
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
