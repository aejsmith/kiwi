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
 * @brief		Signal functions.
 */

#include <lib/string.h>

#include <mm/safe.h>

#include <proc/process.h>
#include <proc/signal.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <object.h>
#include <status.h>

extern void kern_signal_return(void);

/** Check if a signal defaults to being ignored. */
#define signal_dfl_ignore(s)	\
	((s) == SIGCHLD || (s) == SIGURG || (s) == SIGWINCH)

/** Check if a signal defaults to stop. */
#define signal_dfl_stop(s)	\
	((s) == SIGSTOP || (s) == SIGTSTP || (s) == SIGTTIN || (s) == SIGTTOU)

/** Check if a signal defaults to continue. */
#define signal_dfl_cont(s)	((s) == SIGCONT)

/** Check if a signal defaults to core dump. */
#define signal_dfl_core(s)	\
	((s) == SIGQUIT || (s) == SIGILL || (s) == SIGTRAP || \
	 (s) == SIGABRT || (s) == SIGBUS || (s) == SIGFPE || (s) == SIGSEGV)

/** Check if a signal defaults to terminate. */
#define signal_dfl_term(s)	\
	((s) == SIGHUP || (s) == SIGINT || (s) == SIGKILL || \
	 (s) == SIGPIPE || (s) == SIGALRM || (s) == SIGTERM || \
	 (s) == SIGUSR1 || (s) == SIGUSR2)

/** Mask of valid signals.
 * @note		This must be updated if any new signals are added. */
#define VALID_SIGNAL_MASK	0xFFFFFE

/** Send a signal to a thread.
 * @param thread	Thread to send to.
 * @param num		Signal number to send.
 * @param info		If not NULL, extra information for signal. The si_signo
 *			field will be set correctly by this function.
 * @param force		Whether to force delivery, ignoring mask or block. This
 *			is to be used for hardware exceptions, where the signal
 *			must be delivered. */
void signal_send(thread_t *thread, int num, siginfo_t *info, bool force) {
	assert(VALID_SIGNAL_MASK & (1 << num));

	if(info) {
		info->si_signo = num;
	}

	spinlock_lock(&thread->lock);

	/* If we need to force and the signal is ignored or masked, override
	 * this and set to default action or unblock. POSIX allows us to do
	 * this, quote: "If any of the SIGFPE, SIGILL, SIGSEGV, or SIGBUS
	 * signals are generated while they are blocked, the result is undefined,
	 * unless the signal was generated by the kill() function.". Only these
	 * signals should be forced. */
	if(force) {
		thread->signal_mask &= ~(1 << num);
		thread->owner->signal_mask &= ~(1 << num);
		if(thread->owner->signal_act[num].sa_handler == SIG_IGN) {
			thread->owner->signal_act[num].sa_handler = SIG_DFL;
		}
	}

	/* Store information on the signal and mark it as pending. */
	thread->pending_signals |= (1 << num);
	if(info) {
		memcpy(&thread->signal_info[num], info, sizeof(*info));
	} else {
		memset(&thread->signal_info[num], 0, sizeof(thread->signal_info[num]));
		thread->signal_info[num].si_signo = num;
	}

	spinlock_unlock(&thread->lock);

	/* Interrupt the thread if it is currently in interruptible sleep. */
	thread_interrupt(thread);
}

/** Handle pending signals for the current thread. */
void signal_handle_pending(void) {
	sigaction_t *action;
	sigset_t pending;
	int num;

	/* Delay signal delivery during process loading. */
	if(curr_proc->create) {
		return;
	}

	mutex_lock(&curr_proc->lock);
	spinlock_lock(&curr_thread->lock);

	/* Work out the set of pending signals with masks applied. */
	pending = curr_thread->pending_signals;
	pending &= ~(curr_thread->signal_mask | curr_proc->signal_mask);
	if(!pending) {
		spinlock_unlock(&curr_thread->lock);
		mutex_unlock(&curr_proc->lock);
		return;
	}

	for(num = 0; num < NSIG; num++) {
		if(!(pending & (1 << num))) {
			continue;
		}

		curr_thread->pending_signals &= ~(1 << num);

		/* Check if the signal is ignored. */
		action = &curr_proc->signal_act[num];
		if(action->sa_handler == SIG_IGN || (action->sa_handler == SIG_DFL && signal_dfl_ignore(num))) {
			continue;
		}

		/* If not the default action, we must execute a user-mode
		 * handler function. */
		if(action->sa_handler != SIG_DFL) {
			// handle sa flags here!
			kprintf(LOG_WARN, "signal: todo: execute user-mode handler %d\n", num);
			continue;
		}

		/* Unlock while handling default action in case we need to kill
		 * the process. */
		spinlock_unlock(&curr_thread->lock);
		mutex_unlock(&curr_proc->lock);

		/* Handle the default action. */
		if(signal_dfl_term(num)) {
			// FIXME: reason
			process_exit(num);
		} else if(signal_dfl_core(num)) {
			/* TODO: Core dump. */
			// FIXME: reason
			process_exit(num);
		} else if(signal_dfl_stop(num)) {
			/* TODO: Stop process. */
			kprintf(LOG_WARN, "signal: todo: stop process\n");
		} else if(signal_dfl_cont(num)) {
			/* TODO: Continue process. */
			break;
		}

		mutex_lock(&curr_proc->lock);
		spinlock_lock(&curr_thread->lock);
	}

	spinlock_unlock(&curr_thread->lock);
	mutex_unlock(&curr_proc->lock);
}

/** Send a signal.
 * @param handle	Handle to process or thread to send signal to. If -1,
 *			will be sent to the calling thread.
 * @param num		Signal number to send.
 * @return		Status code describing result of the operation. */
status_t kern_signal_send(handle_t handle, int num) {
	object_handle_t *khandle;
	thread_t *thread = NULL;
	process_t *process;
	siginfo_t info;
	status_t ret;

	if(!(VALID_SIGNAL_MASK & (1 << num))) {
		return STATUS_INVALID_ARG;
	}

	memset(&info, 0, sizeof(info));
	info.si_code = SI_USER;
	info.si_pid = curr_proc->id;
	info.si_uid = curr_proc->security.uid;

	if(handle > 0) {
		ret = object_handle_lookup(handle, -1, 0, &khandle);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		switch(khandle->object->type->id) {
		case OBJECT_TYPE_PROCESS:
			process = (process_t *)khandle->object;
			if(!object_handle_rights(khandle, PROCESS_RIGHT_SIGNAL)) {
				object_handle_release(khandle);
				return STATUS_ACCESS_DENIED;
			}

			mutex_lock(&process->lock);

			if(list_empty(&process->threads)) {
				mutex_unlock(&process->lock);
				object_handle_release(khandle);
				return STATUS_NOT_FOUND;
			}

			/* Pick a thread to send to. We pick the first thread
			 * in the process that does not mask the signal. If all
			 * threads mask it, then it is queued to the first
			 * thread. */
			LIST_FOREACH(&process->threads, iter) {
				thread = list_entry(iter, thread_t, owner_link);

				if(!(thread->signal_mask & (1 << num))) {
					break;
				}

				thread = NULL;
			}
			if(!thread) {
				thread = list_entry(process->threads.next, thread_t, owner_link);
			}
			mutex_unlock(&process->lock);
			break;
		case OBJECT_TYPE_THREAD:
			thread = (thread_t *)khandle->object;
			if(!object_handle_rights(khandle, THREAD_RIGHT_SIGNAL)) {
				object_handle_release(khandle);
				return STATUS_ACCESS_DENIED;
			}
			break;
		default:
			object_handle_release(khandle);
			return STATUS_INVALID_HANDLE;
		}

		signal_send(thread, num, &info, false);
		object_handle_release(khandle);
	} else {
		signal_send(curr_thread, num, &info, false);
	}

	return STATUS_SUCCESS;
}

/** Examine and modify the action for a signal.
 * @param num		Signal number to modify.
 * @param newp		If not NULL, pointer to a new action to set.
 * @param oldp		If not NULL, where to store previous action.
 * @return		Status code describing result of the operation. */
status_t kern_signal_action(int num, const sigaction_t *newp, sigaction_t *oldp) {
	sigaction_t kaction;
	status_t ret;

	if(!(VALID_SIGNAL_MASK & (1 << num))) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&curr_proc->lock);

	if(oldp) {
		ret = memcpy_to_user(oldp, &curr_proc->signal_act[num], sizeof(*oldp));
		if(ret != STATUS_SUCCESS) {
			mutex_unlock(&curr_proc->lock);
			return ret;
		}
	}

	if(newp) {
		if(num == SIGKILL || num == SIGSTOP) {
			mutex_unlock(&curr_proc->lock);
			return STATUS_INVALID_ARG;
		}

		ret = memcpy_from_user(&kaction, newp, sizeof(kaction));
		if(ret != STATUS_SUCCESS) {
			mutex_unlock(&curr_proc->lock);
			return ret;
		}

		memcpy(&curr_proc->signal_act[num], &kaction, sizeof(kaction));
	}

	mutex_unlock(&curr_proc->lock);
	return STATUS_SUCCESS;
}

/** Get and set the signal mask.
 * @param flags		How to set the mask (a POSIX action code plus any other
 *			SIGNAL_MASK_* flags).
 * @param newp		If not NULL, new mask to set.
 * @param oldp		If not NULL, where to store previous mask.
 * @return		Status code describing result of the operation. */
status_t kern_signal_mask(int flags, const sigset_t *newp, sigset_t *oldp) {
	sigset_t *target, kset;
	status_t ret;
	int action;

	action = flags & SIGNAL_MASK_ACTION;

	/* Work out the mask to modify. */
	if(flags & SIGNAL_MASK_THREAD) {
		target = &curr_thread->signal_mask;
	} else {
		target = &curr_proc->signal_mask;
	}

	if(oldp) {
		ret = memcpy_to_user(oldp, target, sizeof(*oldp));
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	if(newp) {
		ret = memcpy_from_user(&kset, newp, sizeof(kset));
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		/* Attempts to mask SIGKILL and SIGSTOP are silently ignored. */
		kset &= ~((1 << SIGKILL) | (1 << SIGSTOP));
		kset &= VALID_SIGNAL_MASK;

		switch(action) {
		case SIG_BLOCK:
			*target |= kset;
			break;
		case SIG_UNBLOCK:
			*target &= ~kset;
			break;
		case SIG_SETMASK:
			*target = kset;
			break;
		}
	}

	return STATUS_SUCCESS;
}

/** Get and set the alternate signal stack.
 *
 * Gets and sets the alternate signal stack for the current thread. This stack
 * is used to execute signal handlers with the SA_ONSTACK flag set. The
 * alternate stack is a per-thread attribute. If kern_process_clone() is called,
 * the new process' initial thread inherits the alternate stack from the thread
 * that called kern_process_clone().
 *
 * @param newp		Alternate stack to set (can be NULL).
 * @param oldp		Where to store previous alternate stack (can be NULL).
 *
 * @return		0 on success, -1 on failure.
 */
status_t kern_signal_altstack(const stack_t *newp, stack_t *oldp) {
	stack_t kstack;
	status_t ret;

	if(oldp) {
		ret = memcpy_to_user(oldp, &curr_thread->signal_altstack, sizeof(*oldp));
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	if(newp) {
		ret = memcpy_from_user(&kstack, newp, sizeof(kstack));
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		if(!kstack.ss_size || kstack.ss_flags & ~SS_DISABLE) {
			return STATUS_INVALID_ARG;
		}

		/* Check whether the provided stack range is valid. */
		ret = validate_user_address(kstack.ss_sp, kstack.ss_size);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		memcpy(&curr_thread->signal_altstack, &kstack, sizeof(kstack));
	}

	return STATUS_SUCCESS;
}

/** Return from a signal handler. */
void kern_signal_return(void) {
	fatal("TODO");
}
