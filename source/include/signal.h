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
 * @brief		POSIX signal functions.
 */

#ifndef __SIGNAL_H
#define __SIGNAL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Signal number definitions. Values of default action:
 *  - A: Abnormal termination with core dump.
 *  - T: Abnormal termination.
 *  - I: Ignore.
 *  - S: Stop the process.
 *  - C: Continue the process.
 */
#define SIGHUP		1		/**< Hangup (T). */
#define SIGINT		2		/**< Terminal interrupt signal (T). */
#define SIGQUIT		3		/**< Terminal quit signal (A). */
#define SIGILL		4		/**< Illegal instruction (A). */
#define SIGTRAP		5		/**< Trace trap (A). */
#define SIGABRT		6		/**< Process abort signal (A). */
#define SIGBUS		7		/**< Access to undefined portion of memory object (A). */
#define SIGFPE		8		/**< Erroneous arithmetic operation (A). */
#define SIGKILL		9		/**< Kill (cannot be caught or ignored) (T). */
#define SIGCHLD		10		/**< Child process terminated, stopped or continued (I). */
#define SIGSEGV		11		/**< Invalid memory reference (A). */
#define SIGSTOP		12		/**< Stop executing (cannot be caught or ignored) (S). */
#define SIGPIPE		13		/**< Write on a pipe with nobody to read it (T). */
#define SIGALRM		14		/**< Alarm clock (T). */
#define SIGTERM		15		/**< Termination signal (T). */
#define SIGUSR1		16		/**< User-defined signal 1 (T). */
#define SIGUSR2		17		/**< User-defined signal 2 (T). */
#define SIGCONT		18		/**< Continue execution, if stopped (C). */
#define SIGURG		19		/**< High bandwidth data is available at socket (I). */
#define SIGTSTP		20		/**< Terminal stop signal (S). */
#define SIGTTIN		21		/**< Background process attempting to read (S). */
#define SIGTTOU		22		/**< Background process attempting to write (S). */
#define SIGPOLL		23		/**< File descriptor ready to perform I/O (T). */
#define SIGIO		SIGPOLL		/**< Synonym for SIGPOLL (T). */
#define SIGWINCH	24		/**< Window size change (I). */
#define NSIG		25		/**< Highest signal number. */

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

/** Signal information structure passed to a signal handler. */
typedef struct siginfo {
	int si_signo;			/**< Signal number. */
	int si_code;			/**< Signal code. */
	int si_errno;			/**< If non-zero, an errno value associated with this signal. */
	pid_t si_pid;			/**< Sending process ID. */
	uid_t si_uid;			/**< Real user ID of sending process. */
	void *si_addr;			/**< Address of faulting instruction. */
	int si_status;			/**< Exit value or signal. */
	long si_band;			/**< Band event for SIGIO. */
} siginfo_t;

/** Values for siginfo.si_code for any signal. */
#define SI_USER		1		/**< Signal sent by kill(). */
#define SI_QUEUE	2		/**< Signal sent by the sigqueue(). */
#define SI_TIMER	3		/**< Signal generated by expiration of a timer set by timer_settime(). */
#define SI_ASYNCIO	4		/**< Signal generated by completion of an asynchronous I/O request. */
#define SI_MESGQ	5		/**< Signal generated by arrival of a message on an empty message queue. */

/** Values for siginfo.si_code for SIGILL. */
#define ILL_ILLOPC	10		/**< Illegal opcode. */
#define ILL_ILLOPN	11		/**< Illegal operand. */
#define ILL_ILLADR	12		/**< Illegal addressing mode. */
#define ILL_ILLTRP	13		/**< Illegal trap. */
#define ILL_PRVOPC	14		/**< Privileged opcode. */
#define ILL_PRVREG	15		/**< Privileged register. */
#define ILL_COPROC	16		/**< Coprocessor error. */
#define ILL_BADSTK	17		/**< Internal stack error. */

/** Values for siginfo.si_code for SIGFPE. */
#define FPE_INTDIV	20		/**< Integer divide by zero. */
#define FPE_INTOVF	21		/**< Integer overflow. */
#define FPE_FLTDIV	22		/**< Floating-point divide by zero. */
#define FPE_FLTOVF	23		/**< Floating-point overflow. */
#define FPE_FLTUNDF	24		/**< loating-point underflow. */
#define FPE_FLTRES	25		/**< Floating-point inexact result. */
#define FPE_FLTINV	26		/**< Invalid floating-point operation. */
#define FPE_FLTSUB	27		/**< Subscript out of range. */

/** Values for siginfo.si_code for SIGSEGV. */
#define SEGV_MAPERR	30		/**< Address not mapped to object. */
#define SEGV_ACCERR	31		/**< Invalid permissions for mapped object. */

/** Values for siginfo.si_code for SIGBUS. */
#define BUS_ADRALN	40		/**< Invalid address alignment. */
#define BUS_ADRERR	41		/**< Nonexistent physical address. */
#define BUS_OBJERR	42		/**< Object-specific hardware error. */

/** Values for siginfo.si_code for SIGTRAP. */
#define TRAP_BRKPT	50		/**< Process breakpoint. */
#define TRAP_TRACE	51		/**< Process trace trap. */

/** Values for siginfo.si_code for SIGCHLD. */
#define CLD_EXITED	60		/**< Child has exited. */
#define CLD_KILLED	61		/**< Child has terminated abnormally and did not create a core file. */
#define CLD_DUMPED	62		/**< Child has terminated abnormally and created a core file. */
#define CLD_TRAPPED	63		/**< Traced child has trapped. */
#define CLD_STOPPED	64		/**< Child has stopped. */
#define CLD_CONTINUED	65		/**< Stopped child has continued. */

/** Values for siginfo.si_code for SIGPOLL. */
#define POLL_IN		70		/**< Data input available. */
#define POLL_OUT	71		/**< Output buffers available. */
#define POLL_MSG	72		/**< Input message available. */
#define POLL_ERR	73		/**< I/O error. */
#define POLL_PRI	74		/**< High priority input available. */
#define POLL_HUP	75		/**< Device disconnected. */

/** Structure describing how to handle a signal. */
struct sigaction {
	/** Handler function pointers, or one of the above special values. */
	union {
		/** Old style signal-handler. */
		void (*sa_handler)(int);

		/** Extended signal handler for if SA_SIGINFO is set. */
		void (*sa_sigaction)(int, siginfo_t *, void *);
	};

	sigset_t sa_mask;		/**< Bitmap of signals to block during handler execution. */
	int sa_flags;			/**< Flags controlling signal behaviour. */
};

/** Special signal handler values. */
#define SIG_DFL		((void (*)(int))0)
#define SIG_IGN		((void (*)(int))1)

/** Signal action flags. */
#define SA_NOCLDSTOP	(1<<0)		/**< SIGCHLD won't be generated when child stops or continues. */
#define SA_RESETHAND	(1<<1)		/**< Reset signal to SIG_DFL on entry to signal handler. */
#define SA_RESTART	(1<<2)		/**< Make certain system calls restartable if interrupted. */
#define SA_SIGINFO	(1<<3)		/**< Pass extra information to signal handler. */
#define SA_NOCLDWAIT	(1<<4)		/**< Don't create zombie processes on child death (SIGCHLD only). */
#define SA_NODEFER	(1<<5)		/**< Signal won't be blocked on entry to signal handler. */

/** Value returned from signal() on error. */
#define SIG_ERR		((void (*)(int))-1)

/** Values for the how parameter to sigprocmask(). */
#define SIG_BLOCK	1		/**< Block all signals set in the provided mask. */
#define SIG_SETMASK	2		/**< Replace current mask with provided mask. */
#define SIG_UNBLOCK	3		/**< Unblock all signals set in the provided mask. */

extern const char *const sys_siglist[];

extern int kill(pid_t pid, int num);
/* int killpg(pid_t, int); */
extern void psignal(int sig, const char *s);
/* int pthread_kill(pthread_t, int); */
/* int pthread_sigmask(int, const sigset_t *, sigset_t *); */
extern int raise(int num);
extern int sigaction(int num, const struct sigaction *__restrict act,
                     struct sigaction *__restrict oldact);
extern int sigaddset(sigset_t *set, int num);
extern int sigdelset(sigset_t *set, int num);
extern int sigemptyset(sigset_t *set);
extern int sigfillset(sigset_t *set);
/* int sighold(int); */
/* int sigignore(int); */
/* int siginterrupt(int, int); */
extern int sigismember(const sigset_t *set, int num);
extern void (*signal(int sig, void (*handler)(int)))(int);
/* int sigpause(int); */
/* int sigpending(sigset_t *); */
extern int sigprocmask(int how, const sigset_t *__restrict set, sigset_t *__restrict oset);
/* int sigrelse(int); */
/* void (*sigset(int, void (*)(int)))(int); */
//extern int sigsuspend(const sigset_t *mask);
/* int sigwait(const sigset_t *, int *); */

/* Non-POSIX. */

#ifdef __cplusplus
}
#endif

#endif /* __SIGNAL_H */
