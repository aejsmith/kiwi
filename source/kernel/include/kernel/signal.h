/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX signals.
 *
 * @note		The standard POSIX signal APIs should be used instead
 *			of the APIs defined in this file. These are the kernel-
 *			style APIs used to implement the POSIX APIs.
 */

#ifndef __KERNEL_SIGNAL_H
#define __KERNEL_SIGNAL_H

#include <kernel/types.h>

/**
 * Signal number definitions. Values of default action:
 *  - A: Abnormal termination with core dump.
 *  - T: Abnormal termination.
 *  - I: Ignore.
 *  - S: Stop the process.
 *  - C: Continue the process.
 */
#define SIGHUP			1	/**< Hangup (T). */
#define SIGINT			2	/**< Terminal interrupt signal (T). */
#define SIGQUIT			3	/**< Terminal quit signal (A). */
#define SIGILL			4	/**< Illegal instruction (A). */
#define SIGTRAP			5	/**< Trace trap (A). */
#define SIGABRT			6	/**< Process abort signal (A). */
#define SIGBUS			7	/**< Access to undefined portion of memory object (A). */
#define SIGFPE			8	/**< Erroneous arithmetic operation (A). */
#define SIGKILL			9	/**< Kill (cannot be caught or ignored) (T). */
#define SIGCHLD			10	/**< Child process terminated, stopped or continued (I). */
#define SIGSEGV			11	/**< Invalid memory reference (A). */
#define SIGSTOP			12	/**< Stop executing (cannot be caught or ignored) (S). */
#define SIGPIPE			13	/**< Write on a pipe with nobody to read it (T). */
#define SIGALRM			14	/**< Alarm clock (T). */
#define SIGTERM			15	/**< Termination signal (T). */
#define SIGUSR1			16	/**< User-defined signal 1 (T). */
#define SIGUSR2			17	/**< User-defined signal 2 (T). */
#define SIGCONT			18	/**< Continue execution, if stopped (C). */
#define SIGURG			19	/**< High bandwidth data is available at socket (I). */
#define SIGTSTP			20	/**< Terminal stop signal (S). */
#define SIGTTIN			21	/**< Background process attempting to read (S). */
#define SIGTTOU			22	/**< Background process attempting to write (S). */
#define SIGWINCH		23	/**< Window size change (I). */
#define NSIG			24	/**< Highest signal number. */

/** Signal bitmap type. Must be big enough to hold a bit for each signal. */
typedef uint32_t sigset_t;

/** Type atomically accessible through asynchronous signal handlers. */
typedef volatile int sig_atomic_t;

/** Signal stack information structure. */
typedef struct stack {
	void *ss_sp;			/**< Stack base or pointer. */
	size_t ss_size;			/**< Stack size. */
	int ss_flags;			/**< Flags (unused). */
} stack_t;

/** Structure describing previous machine context on a signal. */
typedef struct mcontext {
	#if defined(__i386__) || defined(__x86_64__)
	unsigned long ax, bx, cx, dx, di, si, bp;
	#ifdef __x86_64__
	unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	#endif
	unsigned long ip, flags, sp;
	#else
	# error "No mcontext_t defined for this architecture"
	#endif
} mcontext_t;

/** Structure containing a saved context. */
typedef struct ucontext {
	struct ucontext *uc_link;	/**< Unused. */
	sigset_t uc_sigmask;		/**< Signals masked when this context is active. */
	stack_t uc_stack;		/**< Stack used by this context. */
	mcontext_t uc_mcontext;		/**< Machine-specific saved context. */
} ucontext_t;

/** Signal information structure passed to a signal handler. */
typedef struct siginfo {
	int si_signo;			/**< Signal number. */
	int si_code;			/**< Signal code. */
	int si_errno;			/**< If non-zero, an errno value associated with this signal. */
	process_id_t si_pid;		/**< Sending process ID. */
	user_id_t si_uid;		/**< Real user ID of sending process. */
	void *si_addr;			/**< Address of faulting instruction. */
	int si_status;			/**< Exit value or signal. */
} siginfo_t;

/** Values for siginfo.si_code for any signal. */
#define SI_USER			1	/**< Signal sent by kill(). */
#define SI_QUEUE		2	/**< Signal sent by sigqueue(). */
#define SI_TIMER		3	/**< Signal generated by expiration of a timer set by timer_settime(). */
#define SI_ASYNCIO		4	/**< Signal generated by completion of an asynchronous I/O request. */
#define SI_MESGQ		5	/**< Signal generated by arrival of a message on an empty message queue. */

/** Values for siginfo.si_code for SIGILL. */
#define ILL_ILLOPC		10	/**< Illegal opcode. */
#define ILL_ILLOPN		11	/**< Illegal operand. */
#define ILL_ILLADR		12	/**< Illegal addressing mode. */
#define ILL_ILLTRP		13	/**< Illegal trap. */
#define ILL_PRVOPC		14	/**< Privileged opcode. */
#define ILL_PRVREG		15	/**< Privileged register. */
#define ILL_COPROC		16	/**< Coprocessor error. */
#define ILL_BADSTK		17	/**< Internal stack error. */

/** Values for siginfo.si_code for SIGFPE. */
#define FPE_INTDIV		20	/**< Integer divide by zero. */
#define FPE_INTOVF		21	/**< Integer overflow. */
#define FPE_FLTDIV		22	/**< Floating-point divide by zero. */
#define FPE_FLTOVF		23	/**< Floating-point overflow. */
#define FPE_FLTUNDF		24	/**< loating-point underflow. */
#define FPE_FLTRES		25	/**< Floating-point inexact result. */
#define FPE_FLTINV		26	/**< Invalid floating-point operation. */
#define FPE_FLTSUB		27	/**< Subscript out of range. */

/** Values for siginfo.si_code for SIGSEGV. */
#define SEGV_MAPERR		30	/**< Address not mapped to object. */
#define SEGV_ACCERR		31	/**< Invalid permissions for mapped object. */

/** Values for siginfo.si_code for SIGBUS. */
#define BUS_ADRALN		40	/**< Invalid address alignment. */
#define BUS_ADRERR		41	/**< Nonexistent physical address. */
#define BUS_OBJERR		42	/**< Object-specific hardware error. */

/** Values for siginfo.si_code for SIGTRAP. */
#define TRAP_BRKPT		50	/**< Process breakpoint. */
#define TRAP_TRACE		51	/**< Process trace trap. */

/** Values for siginfo.si_code for SIGCHLD. */
#define CLD_EXITED		60	/**< Child has exited. */
#define CLD_KILLED		61	/**< Child has terminated abnormally and did not create a core file. */
#define CLD_DUMPED		62	/**< Child has terminated abnormally and created a core file. */
#define CLD_TRAPPED		63	/**< Traced child has trapped. */
#define CLD_STOPPED		64	/**< Child has stopped. */
#define CLD_CONTINUED		65	/**< Stopped child has continued. */

/** Structure describing how to handle a signal. */
typedef struct sigaction {
	/** Handler function pointers, or one of the above special values. */
	union {
		/** Old style signal-handler. */
		void (*sa_handler)(int);

		/** Extended signal handler for if SA_SIGINFO is set. */
		void (*sa_sigaction)(int, siginfo_t *, void *);
	};

	sigset_t sa_mask;		/**< Bitmap of signals to block during handler execution. */
	int sa_flags;			/**< Flags controlling signal behaviour. */

	#if defined(KERNEL) || defined(LIBKERNEL)
	/** Fields for internal use only. */
	void *sa_restorer;		/**< Return address for handler. */
	#endif
} sigaction_t;

/** Special signal handler values. */
#define SIG_DFL			((void (*)(int))0)
#define SIG_IGN			((void (*)(int))1)

/** Signal action flags. */
#define SA_NOCLDSTOP		(1<<0)	/**< SIGCHLD won't be generated when child stops or continues. */
#define SA_ONSTACK		(1<<1)	/**< Execute on alternate stack. */
#define SA_RESETHAND		(1<<2)	/**< Reset signal to SIG_DFL on entry to signal handler. */
#define SA_RESTART		(1<<3)	/**< Make certain system calls restartable if interrupted. */
#define SA_SIGINFO		(1<<4)	/**< Pass extra information to signal handler. */
#define SA_NOCLDWAIT		(1<<5)	/**< Don't create zombie processes on child death (SIGCHLD only). */
#define SA_NODEFER		(1<<6)	/**< Signal won't be blocked on entry to signal handler. */

/** Signal stack flags. */
#define SS_DISABLE		(1<<0)	/**< The stack is currently disabled. */

/** Actions for kern_signal_mask(). */
#define SIG_BLOCK		1	/**< Block all signals set in the provided mask. */
#define SIG_SETMASK		2	/**< Replace current mask with provided mask. */
#define SIG_UNBLOCK		3	/**< Unblock all signals set in the provided mask. */

#endif /* __KERNEL_SIGNAL_H */

#ifndef __POSIX_DEFS_ONLY

#ifndef __KERNEL_SIGNAL_H_
#define __KERNEL_SIGNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Flags for kern_signal_mask(). */
#define SIGNAL_MASK_ACTION	0x3	/**< Mask to get the set action. */
#define SIGNAL_MASK_THREAD	(1<<3)	/**< Operate on the per-thread signal mask. */

extern status_t kern_signal_send(handle_t handle, int num);
extern status_t kern_signal_action(int num, const sigaction_t *newp, sigaction_t *oldp);
extern status_t kern_signal_mask(int flags, const sigset_t *newp, sigset_t *oldp);
extern status_t kern_signal_stack(const stack_t *newp, stack_t *oldp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SIGNAL_H_ */

#else

#undef __POSIX_DEFS_ONLY

#endif /* __POSIX_DEFS_ONLY */
