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
 * @brief		Thread management code.
 */

#include <arch/memory.h>

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <lib/id_alloc.h>
#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>
#include <sync/waitq.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <status.h>
#include <time.h>

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);
extern void thread_wake(thread_t *thread);

/** Tree of all threads. */
static AVL_TREE_DECLARE(thread_tree);
static RWLOCK_DECLARE(thread_tree_lock);

/** Thread ID allocator. */
static id_alloc_t thread_id_allocator;

/** Thread structure cache. */
static slab_cache_t *thread_cache;

/** Dead thread queue information. */
static LIST_DECLARE(dead_threads);
static SPINLOCK_DECLARE(dead_thread_lock);
static SEMAPHORE_DECLARE(dead_thread_sem, 0);

/** Constructor for thread objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void thread_cache_ctor(void *obj, void *data) {
	thread_t *thread = (thread_t *)obj;

	spinlock_init(&thread->lock, "thread_lock");
	list_init(&thread->runq_link);
	list_init(&thread->waitq_link);
	list_init(&thread->owner_link);
	notifier_init(&thread->death_notifier, thread);
}

/** Thread entry function wrapper. */
static void thread_trampoline(void) {
	/* Upon switching to a newly-created thread's context, execution will
	 * jump to this function, rather than going back to the scheduler.
	 * It is therefore necessary to perform post-switch tasks now. */
	sched_post_switch(true);

	dprintf("thread: entered thread %" PRId32 "(%s) on CPU %" PRIu32 "\n",
		curr_thread->id, curr_thread->name, curr_cpu->id);

	/* Set the last time to now so that accounting information is correct. */
	curr_thread->last_time = system_time();

	/* Run the thread's main function and then exit when it returns. */
	curr_thread->entry(curr_thread->arg1, curr_thread->arg2);
	thread_exit();
}

/** Entry function for a userspace thread.
 * @param _args		Argument structure pointer.
 * @param arg2		Unused. */
void thread_uspace_trampoline(void *_args, void *arg2) {
	thread_uspace_args_t *args = _args;
	ptr_t entry, sp, arg;

	entry = args->entry;
	sp = args->sp;
	arg = args->arg;
	kfree(args);

	thread_arch_enter_userspace(entry, sp, arg);
}

/** Dead thread reaper.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void thread_reaper(void *arg1, void *arg2) {
	thread_t *thread;

	while(true) {
		semaphore_down(&dead_thread_sem);

		/* Take the next thread off the list. */
		spinlock_lock(&dead_thread_lock);
		assert(!list_empty(&dead_threads));
		thread = list_entry(dead_threads.next, thread_t, runq_link);
		list_remove(&thread->runq_link);
		spinlock_unlock(&dead_thread_lock);

		/* Remove from thread tree. */
		rwlock_write_lock(&thread_tree_lock);
		avl_tree_remove(&thread_tree, &thread->tree_link);
		rwlock_unlock(&thread_tree_lock);

		/* Detach from its owner. */
		process_detach(thread);

		/* Now clean up the thread. */
		kheap_free(thread->kstack, KSTACK_SIZE);
		context_destroy(&thread->context);
		thread_arch_destroy(thread);
		if(thread->fpu) {
			fpu_context_destroy(thread->fpu);
		}
		notifier_clear(&thread->death_notifier);
		object_destroy(&thread->obj);

		/* Deallocate the thread ID. */
		id_alloc_release(&thread_id_allocator, thread->id);

		dprintf("thread: destroyed thread %" PRId32 "(%s) (thread: %p)\n",
			thread->id, thread->name, thread);

		slab_cache_free(thread_cache, thread);
	}
}

/** Closes a handle to a thread.
 * @param handle	Handle being closed. */
static void thread_object_close(object_handle_t *handle) {
	thread_destroy((thread_t *)handle->object);
}

/** Signal that a thread is being waited for.
 * @param handle	Handle to thread.
 * @param event		Event to wait for.
 * @param sync		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t thread_object_wait(object_handle_t *handle, int event, void *sync) {
	thread_t *thread = (thread_t *)handle->object;

	switch(event) {
	case THREAD_EVENT_DEATH:
		if(thread->state == THREAD_DEAD) {
			object_wait_signal(sync);
		} else {
			notifier_register(&thread->death_notifier, object_wait_notifier, sync);
		}
		return STATUS_SUCCESS;
	default:
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a thread.
 * @param handle	Handle to thread.
 * @param event		Event to wait for.
 * @param sync		Internal data pointer. */
static void thread_object_unwait(object_handle_t *handle, int event, void *sync) {
	thread_t *thread = (thread_t *)handle->object;

	switch(event) {
	case THREAD_EVENT_DEATH:
		notifier_unregister(&thread->death_notifier, object_wait_notifier, sync);
		break;
	}
}

/** Thread object type. */
static object_type_t thread_object_type = {
	.id = OBJECT_TYPE_THREAD,
	.close = thread_object_close,
	.wait = thread_object_wait,
	.unwait = thread_object_unwait,
};

/** Wake up a sleeping thread.
 * @note		Thread and its wait queue should be locked.
 * @param thread	Thread to wake up. */
void thread_wake(thread_t *thread) {
	assert(thread->state == THREAD_SLEEPING);
	assert(spinlock_held(&thread->lock));
	assert(spinlock_held(&thread->waitq->lock));

	/* Stop the timer. */
	timer_stop(&thread->sleep_timer);

	/* Remove the thread from the queue and wake it up. */
	list_remove(&thread->waitq_link);
	thread->waitq = NULL;
	thread->interruptible = false;

	thread->state = THREAD_READY;
	sched_thread_insert(thread);
}

/** Wire a thread to its current CPU.
 *
 * Increases the wire count of a thread to ensure that it will not be
 * migrated to another CPU.
 *
 * @param thread	Thread to wire.
 */
void thread_wire(thread_t *thread) {
	if(thread) {
		spinlock_lock(&thread->lock);
		thread->wire_count++;
		spinlock_unlock(&thread->lock);
	}
}

/** Unwire a thread.
 *
 * Decreases the wire count of a thread. If the count reaches 0, the thread
 * will be unwired and able to migrate again.
 *
 * @param thread	Thread to unwire.
 */
void thread_unwire(thread_t *thread) {
	if(thread) {
		spinlock_lock(&thread->lock);
		if(thread->wire_count == 0) {
			fatal("Calling unwire when thread already unwired");
		}
		thread->wire_count--;
		spinlock_unlock(&thread->lock);
	}
}

/** Interrupt a thread.
 *
 * If the specified thread is in interruptible sleep, causes it to be woken and
 * to return an error from the sleep call.
 *
 * @param thread	Thread to interrupt.
 *
 * @return		Whether the thread was interrupted.
 */
bool thread_interrupt(thread_t *thread) {
	bool ret = false;
	waitq_t *queue;

	spinlock_lock(&thread->lock);

	if(thread->state == THREAD_SLEEPING && thread->interruptible) {
		thread->context = thread->sleep_context;
		queue = thread->waitq;
		spinlock_lock(&queue->lock);
		thread_wake(thread);
		spinlock_unlock(&queue->lock);
		ret = true;
	}

	spinlock_unlock(&thread->lock);
	return ret;
}

/** Request a thread to terminate.
 *
 * Ask a userspace thread to terminate as soon as possible (upon next exit from
 * the kernel). If the thread is currently in interruptible sleep, it will be
 * interrupted. You cannot terminate a kernel thread.
 *
 * @param thread	Thread to kill.
 */
void thread_kill(thread_t *thread) {
	waitq_t *queue;

	spinlock_lock(&thread->lock);
	if(thread->owner != kernel_proc) {
		thread->killed = true;

		/* Interrupt the thread if it is in interruptible sleep. */
		if(thread->state == THREAD_SLEEPING && thread->interruptible) {
			thread->context = thread->sleep_context;
			queue = thread->waitq;
			spinlock_lock(&queue->lock);
			thread_wake(thread);
			spinlock_unlock(&queue->lock);
		}

		/* If the thread is on a different CPU, send the CPU an IPI
		 * so that it will check the thread killed state. */
		if(thread->state == THREAD_RUNNING && thread->cpu != curr_cpu) {
			ipi_send(thread->cpu->id, NULL, 0, 0, 0, 0, 0);
		}
	}
	spinlock_unlock(&thread->lock);
}

/** Rename a thread.
 * @param thread	Thread to rename.
 * @param name		New name for the thread. */
void thread_rename(thread_t *thread, const char *name) {
	spinlock_lock(&thread->lock);
	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;
	spinlock_unlock(&thread->lock);
}

/** Perform tasks necessary when a thread is entering the kernel. */
void thread_at_kernel_entry(void) {
	useconds_t now;

	/* Update accounting information. */
	now = system_time();
	curr_thread->user_time += now - curr_thread->last_time;
	curr_thread->last_time = now;
}

/** Perform tasks necessary when a thread is returning to userspace. */
void thread_at_kernel_exit(void) {
	useconds_t now;

	/* Update accounting information. */
	now = system_time();
	curr_thread->kernel_time += now - curr_thread->last_time;
	curr_thread->last_time = now;

	/* Terminate the thread if killed. */
	if(curr_thread->killed) {
		thread_exit();
	}
}

/** Terminate the current thread.
 * @note		Does not return. */
void thread_exit(void) {
	if(curr_thread->ustack_size) {
		vm_unmap(curr_proc->aspace, curr_thread->ustack, curr_thread->ustack_size);
	}

	curr_thread->state = THREAD_DEAD;
	notifier_run(&curr_thread->death_notifier, NULL, true);

	sched_yield();
	fatal("Shouldn't get here");
}

/** Lookup a running thread without taking the tree lock.
 * @note		Newly created and dead threads are ignored.
 * @note		This function should only be used within KDBG. Use
 *			thread_lookup() outside of KDBG.
 * @param id		ID of the thread to find.
 * @return		Pointer to thread found, or NULL if not found. */
thread_t *thread_lookup_unsafe(thread_id_t id) {
	thread_t *thread = avl_tree_lookup(&thread_tree, id);
	return (thread && (thread->state == THREAD_DEAD || thread->state == THREAD_CREATED)) ? NULL : thread;
}

/** Lookup a running thread.
 * @note		Newly created and dead threads are ignored.
 * @param id		ID of the thread to find.
 * @return		Pointer to thread found, or NULL if not found. */
thread_t *thread_lookup(thread_id_t id) {
	thread_t *ret;

	rwlock_read_lock(&thread_tree_lock);
	ret = thread_lookup_unsafe(id);
	rwlock_unlock(&thread_tree_lock);

	return ret;
}

/** Create a new kernel-mode thread.
 *
 * Creates a new thread that will begin execution at the given kernel-mode
 * address and places it in the Created state. The thread must be started with
 * thread_run().
 *
 * @param name		Name to give the thread.
 * @param owner		Process that the thread should belong to (if NULL,
 *			the thread will belong to the kernel process).
 * @param flags		Flags for the thread.
 * @param entry		Entry function for the thread.
 * @param arg1		First argument to pass to entry function.
 * @param arg2		Second argument to pass to entry function.
 * @param security	Security attributes for the thread (if NULL, default
 *			security attributes will be constructed).
 * @param threadp	Where to store pointer to thread structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t thread_create(const char *name, process_t *owner, int flags, thread_func_t entry,
                       void *arg1, void *arg2, object_security_t *security,
                       thread_t **threadp) {
	object_security_t dsecurity = { -1, -1, NULL };
	object_acl_t acl;
	thread_t *thread;
	status_t ret;

	if(!name || !threadp) {
		return STATUS_INVALID_ARG;
	}

	if(!owner) {
		owner = kernel_proc;
	}

	if(security) {
		ret = object_security_validate(security, NULL);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		dsecurity.uid = security->uid;
		dsecurity.gid = security->gid;
		dsecurity.acl = security->acl;
	}

	/* If an ACL is not given, construct a default ACL. */
	if(!dsecurity.acl) {
		object_acl_init(&acl);
		object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0, THREAD_RIGHT_QUERY);
		dsecurity.acl = &acl;
	}

	/* Allocate a thread structure from the cache. The thread constructor
	 * caches a kernel stack with the thread for us. */
	thread = slab_cache_alloc(thread_cache, MM_SLEEP);

	/* Allocate an ID for the thread. */
	thread->id = id_alloc_get(&thread_id_allocator);
	if(thread->id < 0) {
		slab_cache_free(thread_cache, thread);
		return STATUS_THREAD_LIMIT;
	}

	strncpy(thread->name, name, THREAD_NAME_MAX);
	thread->name[THREAD_NAME_MAX - 1] = 0;

	/* Allocate a kernel stack and initialise the thread context. */
	thread->kstack = kheap_alloc(KSTACK_SIZE, MM_SLEEP);
	context_init(&thread->context, (ptr_t)thread_trampoline, thread->kstack);

	/* Initialise architecture-specific data. */
	ret = thread_arch_init(thread);
	if(ret != STATUS_SUCCESS) {
		kheap_free(thread->kstack, KSTACK_SIZE);
		id_alloc_release(&thread_id_allocator, thread->id);
		slab_cache_free(thread_cache, thread);
		return ret;
	}

	/* Initially set the CPU to NULL - the thread will be assigned to a
	 * CPU when thread_run() is called on it. */
	thread->cpu = NULL;

	object_init(&thread->obj, &thread_object_type, &dsecurity, NULL);
	refcount_set(&thread->count, 1);
	thread->fpu = NULL;
	thread->wire_count = 0;
	thread->killed = false;
	thread->ustack = NULL;
	thread->ustack_size = 0;
	thread->flags = flags;
	thread->priority = 0;
	thread->timeslice = 0;
	thread->preempt_off = 0;
	thread->preempt_missed = false;
	thread->waitq = NULL;
	thread->interruptible = false;
	thread->timed_out = false;
	thread->last_time = 0;
	thread->kernel_time = 0;
	thread->user_time = 0;
	thread->in_usermem = false;
	thread->state = THREAD_CREATED;
	thread->entry = entry;
	thread->arg1 = arg1;
	thread->arg2 = arg2;

	/* Add the thread to the owner. */
	process_attach(owner, thread);

	/* Add to the thread tree. */
	rwlock_write_lock(&thread_tree_lock);
	avl_tree_insert(&thread_tree, &thread->tree_link, thread->id, thread);
	rwlock_unlock(&thread_tree_lock);

	*threadp = thread;

	dprintf("thread: created thread %" PRId32 "(%s) (thread: %p, owner: %p)\n",
		thread->id, thread->name, thread, owner);
	return STATUS_SUCCESS;
}

/** Run a newly-created thread.
 *
 * Moves a newly created thread into the Ready state and places it on the
 * run queue of the current CPU.
 *
 * @todo		If the thread is not tied to the current CPU, pick the
 *			best CPU for it to run on.
 *
 * @param thread	Thread to run.
 */
void thread_run(thread_t *thread) {
	spinlock_lock(&thread->lock);

	assert(thread->state == THREAD_CREATED);

	thread->state = THREAD_READY;
	thread->cpu = curr_cpu;
	sched_thread_insert(thread);

	spinlock_unlock(&thread->lock);
}

/** Destroy a thread.
 *
 * Decreases the reference count of a thread, and queues it for deletion if it
 * reaches 0. Do NOT use on threads that are running, for this use thread_kill()
 * or call thread_exit() from the thread.
 *
 * @param thread	Thread to destroy.
 */
void thread_destroy(thread_t *thread) {
	spinlock_lock(&thread->lock);

	if(refcount_dec(&thread->count) > 0) {
		spinlock_unlock(&thread->lock);
		return;
	}

	dprintf("thread: queueing thread %" PRId32 "(%s) for deletion (owner: %" PRId32 ")\n",
		thread->id, thread->name, thread->owner->id);

	assert(list_empty(&thread->runq_link));
	assert(thread->state == THREAD_CREATED || thread->state == THREAD_DEAD);

	/* Queue for deletion by the thread reaper. */
	spinlock_lock(&dead_thread_lock);
	list_append(&dead_threads, &thread->runq_link);
	semaphore_up(&dead_thread_sem, 1);
	spinlock_unlock(&dead_thread_lock);

	spinlock_unlock(&thread->lock);
}

/** Kill a thread.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_kill(int argc, char **argv) {
	thread_t *thread;
	unative_t tid;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<thread ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Schedules a currently running thread to be killed once KDBG exits.\n");
		kprintf(LOG_NONE, "Note that this has no effect on kernel threads.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &tid, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	} else if(!(thread = thread_lookup_unsafe(tid))) {
		kprintf(LOG_NONE, "Invalid thread ID.\n");
		return KDBG_FAIL;
	}

	thread_kill(thread);
	return KDBG_OK;
}

/** Print information about a thread.
 * @param thread	Thread to print.
 * @param level		Log level. */
static inline void thread_dump(thread_t *thread, int level) {
	kprintf(level, "%-5" PRId32 "%s ", thread->id,
	        (thread == curr_thread) ? "*" : " ");

	switch(thread->state) {
	case THREAD_CREATED:	kprintf(level, "Created      "); break;
	case THREAD_READY:	kprintf(level, "Ready        "); break;
	case THREAD_RUNNING:	kprintf(level, "Running      "); break;
	case THREAD_SLEEPING:
		kprintf(level, "Sleeping ");
		if(thread->interruptible) {
			kprintf(level, "(I) ");
		} else {
			kprintf(level, "    ");
		}
		break;
	case THREAD_DEAD:	kprintf(level, "Dead         "); break;
	default:		kprintf(level, "Bad          "); break;
	}

	kprintf(level, "%-4" PRIu32 " %-4d %-4zu %-5d %-20s %-5" PRId32 " %s\n",
	        (thread->cpu) ? thread->cpu->id : 0, thread->wire_count, thread->priority,
	        thread->flags, (thread->waitq) ? thread->waitq->name : "None",
	        thread->owner->id, thread->name);
}

/** Dump a list of threads.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_thread(int argc, char **argv) {
	process_t *process;
	thread_t *thread;
	unative_t pid;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<process ID>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all threads, or a list of threads within a process\n");
		kprintf(LOG_NONE, "if given a process ID. The ID is given as an expression.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	kprintf(LOG_NONE, "ID     State        CPU  Wire Prio Flags Waiting On           Owner Name\n");
	kprintf(LOG_NONE, "==     =====        ===  ==== ==== ===== ==========           ===== ====\n");

	if(argc == 2) {
		/* Find the process ID. */
		if(kdbg_parse_expression(argv[1], &pid, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(process = process_lookup_unsafe(pid))) {
			kprintf(LOG_NONE, "Invalid process ID.\n");
			return KDBG_FAIL;
		}

		LIST_FOREACH(&process->threads, iter) {
			thread = list_entry(iter, thread_t, owner_link);
			thread_dump(thread, LOG_NONE);
		}
	} else {
		AVL_TREE_FOREACH(&thread_tree, iter) {
			thread = avl_tree_entry(iter, thread_t);
			thread_dump(thread, LOG_NONE);
		}
	}

	return KDBG_OK;
}

/** Initialise the thread system. */
void __init_text thread_init(void) {
	/* Initialise the thread ID allocator. */
	id_alloc_init(&thread_id_allocator, 65535);

	/* Create the thread slab cache. */
	thread_cache = slab_cache_create("thread_cache", sizeof(thread_t), 0,
	                                 thread_cache_ctor, NULL, NULL, NULL, 0,
	                                 MM_FATAL);
}

/** Create the thread reaper. */
void __init_text thread_reaper_init(void) {
	thread_t *thread;
	status_t ret;

	ret = thread_create("thread_reaper", NULL, 0, thread_reaper, NULL, NULL, NULL, &thread);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create thread reaper (%d)", ret);
	}
	thread_run(thread);
}

/** Create a new thread.
 * @param name		Name of the thread to create.
 * @param stack		Pointer to base of stack to use for thread. If NULL,
 *			then a new stack will be allocated.
 * @param stacksz	Size of stack. If a stack is provided, then this should
 *			be the size of that stack. Otherwise, it is used as the
 *			size of the stack to create - if it is zero then a
 *			stack of the default size will be allocated.
 * @param func		Function to execute.
 * @param arg		Argument to pass to thread.
 * @param handlep	Where to store handle to the thread (can be NULL).
 * @return		Status code describing result of the operation. */
status_t kern_thread_create(const char *name, void *stack, size_t stacksz, void (*func)(void *),
                            void *arg, const object_security_t *security, object_rights_t rights,
                            handle_t *handlep) {
	object_security_t ksecurity = { -1, -1, NULL };
	thread_uspace_args_t *args;
	thread_t *thread = NULL;
	handle_t handle = -1;
	status_t ret;
	char *kname;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	ret = strndup_from_user(name, THREAD_NAME_MAX, &kname);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Create arguments structure. */
	args = kmalloc(sizeof(thread_uspace_args_t), MM_SLEEP);
	args->entry = (ptr_t)func;
	args->arg = (ptr_t)arg;

	if(security) {
		ret = object_security_from_user(&ksecurity, security, false);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Create the thread, but do not run it yet. We attempt to create the
	 * handle to the thread before running it as this allows us to
	 * terminate it if not successful. */
	ret = thread_create(kname, curr_proc, 0, thread_uspace_trampoline, args, NULL, &ksecurity, &thread);
	object_security_destroy(&ksecurity);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	/* Create a handle to the thread if necessary. */
	if(handlep) {
		refcount_inc(&thread->count);
		ret = object_handle_create(&thread->obj, NULL, rights, NULL, 0, NULL, &handle, handlep);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	}

	/* Create a userspace stack. TODO: Stack direction! */
	if(stack) {
		args->sp = (ptr_t)stack + stacksz;
	} else {
		if(stacksz) {
			stacksz = ROUND_UP(stacksz, PAGE_SIZE);
		} else {
			stacksz = USTACK_SIZE;
		}

		ret = vm_map(curr_proc->aspace, 0, stacksz,
		             VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE | VM_MAP_STACK,
		             NULL, 0, &thread->ustack);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
		thread->ustack_size = stacksz;
		args->sp = thread->ustack + stacksz;
	}

	thread_run(thread);
	kfree(kname);
	return ret;
fail:
	if(handle >= 0) {
		/* This will handle thread destruction. */
		object_handle_detach(NULL, handle);
	} else if(thread) {
		thread_destroy(thread);
	}
	kfree(args);
	kfree(kname);
	return ret;
}

/** Open a handle to a thread.
 * @param id		ID of the thread to open.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to thread.
 * @return		Status code describing result of the operation. */
status_t kern_thread_open(thread_id_t id, object_rights_t rights, handle_t *handlep) {
	thread_t *thread;
	status_t ret;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	rwlock_read_lock(&thread_tree_lock);

	thread = thread_lookup_unsafe(id);
	if(!thread) {
		rwlock_unlock(&thread_tree_lock);
		return STATUS_NOT_FOUND;
	}

	refcount_inc(&thread->count);
	rwlock_unlock(&thread_tree_lock);

	ret = object_handle_open(&thread->obj, NULL, rights, NULL, 0, NULL, NULL, handlep);
	if(ret != STATUS_SUCCESS) {
		thread_destroy(thread);
	}
	return ret;
}

/** Get the ID of a thread.
 *
 * Gets the ID of the thread referred to by a handle. If the handle is
 * specified as -1, then the ID of the calling thread will be returned.
 *
 * @param handle	Handle for thread to get ID of.
 *
 * @return		Thread ID, or -1 if the handle is invalid.
 */
thread_id_t kern_thread_id(handle_t handle) {
	object_handle_t *khandle;
	thread_id_t id = -1;
	thread_t *thread;

	if(handle < 0) {
		id = curr_thread->id;
	} else if(object_handle_lookup(handle, OBJECT_TYPE_THREAD, 0, &khandle) == STATUS_SUCCESS) {
		thread = (thread_t *)khandle->object;
		id = thread->id;
		object_handle_release(khandle);
	}

	return id;
}

/** Perform operations on a thread.
 * @param handle	Handle to thread, or -1 to operate on the calling thread.
 * @param action	Action to perform.
 * @param in		Pointer to input buffer.
 * @param out		Pointer to output buffer.
 * @return		Status code describing result of the operation. */
status_t kern_thread_control(handle_t handle, int action, const void *in, void *out) {
	object_handle_t *khandle = NULL;
	thread_t *thread;
	status_t ret;

	if(handle < 0) {
		thread = curr_thread;
	} else {
		ret = object_handle_lookup(handle, OBJECT_TYPE_THREAD, 0, &khandle);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		thread = (thread_t *)khandle->object;
	}

	switch(action) {
	case THREAD_SET_TLS_ADDR:
		/* Can only set TLS address of current process. */
		if(khandle) {
			ret = STATUS_NOT_SUPPORTED;
			goto out;
		}

		ret = thread_arch_set_tls_addr(thread, (ptr_t)in);
		break;
	}
out:
	if(khandle) {
		object_handle_release(khandle);
	}
	return ret;
}

/** Query the exit status of a thread.
 * @param handle	Handle to thread.
 * @param statusp	Where to store exit status of thread.
 * @return		Status code describing result of the operation. */
status_t kern_thread_status(handle_t handle, int *statusp) {
	object_handle_t *khandle;
	thread_t *thread;
	status_t ret;

	ret = object_handle_lookup(handle, OBJECT_TYPE_THREAD, THREAD_RIGHT_QUERY, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	thread = (thread_t *)khandle->object;

	if(thread->state != THREAD_DEAD) {
		object_handle_release(khandle);
		return STATUS_STILL_RUNNING;
	}

	ret = memcpy_to_user(statusp, &thread->status, sizeof(int));
	object_handle_release(khandle);
	return ret;
}

/** Terminate the calling thread.
 * @param status	Exit status code. */
void kern_thread_exit(int status) {
	curr_thread->status = status;
	thread_exit();
}

/** Sleep for a certain amount of time.
 * @param us		Number of microseconds to sleep for. Must be 0 or
 *			higher.
 * @param remp		If not NULL, the number of microseconds remaining will
 *			be stored here if the wait is interrupted.
 * @return		Status code describing result of the operation. */
status_t kern_thread_usleep(useconds_t us, useconds_t *remp) {
	useconds_t begin, elapsed, rem;
	status_t ret;

	if(us < 0) {
		return STATUS_INVALID_ARG;
	}

	/* FIXME: The method getting remaining time isn't quite accurate. */
	begin = system_time();
	ret = usleep_etc(us, SYNC_INTERRUPTIBLE);
	if(ret == STATUS_INTERRUPTED && remp) {
		elapsed = system_time() - begin;
		if(elapsed < us) {
			rem = us - elapsed;
			memcpy_to_user(remp, &rem, sizeof(rem));
		} else {
			ret = STATUS_SUCCESS;
		}
	}
	return ret;
}
