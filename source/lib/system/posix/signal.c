/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               POSIX signal functions.
 *
 * TODO:
 *  - Support for pthread signals. Currently, the first thread that installs a
 *    signal handler will be the one that receives all signals, but this breaks
 *    once we can set masks per-thread. I think the way to do this is to set up
 *    a separate signal handler thread that initially receives signals, and then
 *    internally distributes them to threads based on the per-thread masks.
 */

#include <core/utility.h>

#include <kernel/condition.h>
#include <kernel/exception.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <services/posix_service.h>

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "posix/posix.h"

/** Mapping of kernel exceptions to POSIX signals. */
static int posix_exception_signals[] = {
    [EXCEPTION_ADDR_UNMAPPED]       = SIGSEGV,
    [EXCEPTION_ACCESS_VIOLATION]    = SIGSEGV,
    [EXCEPTION_STACK_OVERFLOW]      = SIGSEGV,
    [EXCEPTION_PAGE_ERROR]          = SIGBUS,
    [EXCEPTION_INVALID_ALIGNMENT]   = SIGBUS,
    [EXCEPTION_INVALID_INSTRUCTION] = SIGILL,
    [EXCEPTION_INT_DIV_ZERO]        = SIGFPE,
    [EXCEPTION_INT_OVERFLOW]        = SIGFPE,
    [EXCEPTION_FLOAT_DIV_ZERO]      = SIGFPE,
    [EXCEPTION_FLOAT_OVERFLOW]      = SIGFPE,
    [EXCEPTION_FLOAT_UNDERFLOW]     = SIGFPE,
    [EXCEPTION_FLOAT_PRECISION]     = SIGFPE,
    [EXCEPTION_FLOAT_DENORMAL]      = SIGFPE,
    [EXCEPTION_FLOAT_INVALID]       = SIGFPE,
    [EXCEPTION_BREAKPOINT]          = SIGTRAP,
    [EXCEPTION_ABORT]               = SIGABRT,
};

/** Mapping of kernel exceptions to POSIX signal codes. */
static int posix_exception_codes[] = {
    [EXCEPTION_ADDR_UNMAPPED]       = SEGV_MAPERR,
    [EXCEPTION_ACCESS_VIOLATION]    = SEGV_ACCERR,
    [EXCEPTION_STACK_OVERFLOW]      = SEGV_MAPERR,
    [EXCEPTION_PAGE_ERROR]          = BUS_OBJERR,
    [EXCEPTION_INVALID_ALIGNMENT]   = BUS_ADRALN,
    [EXCEPTION_INVALID_INSTRUCTION] = ILL_ILLOPC,
    [EXCEPTION_INT_DIV_ZERO]        = FPE_INTDIV,
    [EXCEPTION_INT_OVERFLOW]        = FPE_INTOVF,
    [EXCEPTION_FLOAT_DIV_ZERO]      = FPE_FLTDIV,
    [EXCEPTION_FLOAT_OVERFLOW]      = FPE_FLTOVF,
    [EXCEPTION_FLOAT_UNDERFLOW]     = FPE_FLTUND,
    [EXCEPTION_FLOAT_PRECISION]     = FPE_FLTRES,
    [EXCEPTION_FLOAT_DENORMAL]      = FPE_FLTUND,
    [EXCEPTION_FLOAT_INVALID]       = FPE_FLTINV,
    [EXCEPTION_BREAKPOINT]          = TRAP_BRKPT,
    [EXCEPTION_ABORT]               = 0,
};

/** Lock for signal state. This should be locked before the service lock. */
static CORE_MUTEX_DEFINE(posix_signal_lock);

/** Begin a signal guard and take the signal lock. */
#define SCOPED_SIGNAL_LOCK() \
    POSIX_SCOPED_SIGNAL_GUARD(scoped_guard); \
    CORE_MUTEX_SCOPED_LOCK(scoped_lock, &posix_signal_lock);

/** Condition object for signal notifications. */
static handle_t posix_signal_condition = INVALID_HANDLE;

/** Signal handler state. */
typedef struct posix_signal {
    sigaction_t action;
    uint32_t disposition;
} posix_signal_t;

static posix_signal_t posix_signals[NSIG] = {};

/** Bitmap of exceptions that have the POSIX handler installed. */
static uint32_t posix_exceptions_installed = 0;

/** Current (process-wide) signal mask. */
static sigset_t posix_signal_mask = 0;

/** Current signal guard state for the current thread. */
static __thread struct {
    uint32_t prev_ipl;
    uint32_t count;
} posix_signal_guard = {};

/**
 * IPC wrappers.
 */

static bool get_signal_condition_request(core_connection_t *conn, handle_t *_handle) {
    core_message_t *request = core_message_create_request(POSIX_REQUEST_GET_SIGNAL_CONDITION, 0, 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_get_signal_condition_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    if (reply_err == 0) {
        *_handle = core_message_detach_handle(reply);
        libsystem_assert(*_handle != INVALID_HANDLE);
    }

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

static bool get_pending_signal_request(core_connection_t *conn, siginfo_t *_info) {
    core_message_t *request = core_message_create_request(POSIX_REQUEST_GET_PENDING_SIGNAL, 0, 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_get_pending_signal_t *reply_data = core_message_data(reply);
    memcpy(_info, &reply_data->info, sizeof(*_info));

    core_message_destroy(reply);
    return true;
}

static bool set_signal_action_request(core_connection_t *conn, int32_t num, uint32_t disposition, uint32_t flags) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_SET_SIGNAL_ACTION, sizeof(posix_request_set_signal_action_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_set_signal_action_t *request_data = core_message_data(request);
    request_data->num         = num;
    request_data->disposition = disposition;
    request_data->flags       = flags;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_set_signal_action_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;
    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

static bool set_signal_mask_request(core_connection_t *conn, uint32_t mask) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_SET_SIGNAL_MASK, sizeof(posix_request_set_signal_mask_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_set_signal_mask_t *request_data = core_message_data(request);
    request_data->mask = mask;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_set_signal_mask_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;
    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

static bool kill_request(core_connection_t *conn, pid_t pid, int num) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_KILL, sizeof(posix_request_kill_t),
        CORE_MESSAGE_SEND_SECURITY);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_kill_t *request_data = core_message_data(request);
    request_data->pid = pid;
    request_data->num = num;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_kill_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;
    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/**
 * Internal implementation details.
 */

static bool set_signal_action(core_connection_t *conn, int32_t num, uint32_t disposition, uint32_t flags);

static sigset_t make_valid_sigmask(sigset_t mask) {
    /* Restrict to valid signals. SIGKILL and SIGSTOP cannot be masked and
     * should be silently ignored. */
    mask &= ((1 << NSIG) - 1);
    mask &= ~(1 << SIGKILL);
    mask &= ~(1 << SIGSTOP);
    return mask;
}

/**
 * Handle a signal. When called, posix_signal_lock should be held, and the
 * POSIX service should have been obtained. Both will be released when this
 * function returns.
 */
static void handle_signal(core_connection_t *conn, siginfo_t *info, thread_context_t *ctx) {
    /* Take a copy of the current signal action. We must not keep the lock held
     * around calling the handler, because it's legal for the handler to do
     * something like longjmp() away and never return here. */
    sigaction_t action;
    memcpy(&action, &posix_signals[info->si_signo], sizeof(action));

    /* If we have SA_RESETHAND, restore default action. */
    if (action.sa_flags & SA_RESETHAND) {
        if (set_signal_action(conn, info->si_signo, POSIX_SIGNAL_DISPOSITION_DEFAULT, 0)) {
            memset(&posix_signals[info->si_signo], 0, sizeof(*posix_signals));
        } else {
            libsystem_log(CORE_LOG_ERROR, "failed to reset handler while handling signal %d", info->si_signo);
        }
    }

    /* See if we need to change the mask. */
    sigset_t prev_mask = posix_signal_mask;
    sigset_t mask      = posix_signal_mask;

    if (!(action.sa_flags & SA_NODEFER))
        mask |= (1 << info->si_signo);

    mask |= make_valid_sigmask(action.sa_mask);

    if (mask != prev_mask) {
        if (set_signal_mask_request(conn, mask)) {
            posix_signal_mask = mask;
        } else {
            libsystem_log(CORE_LOG_ERROR, "failed to update signal mask while handling signal %d", info->si_signo);
        }
    }

    posix_service_put();
    core_mutex_unlock(&posix_signal_lock);

    /*
     * Restore the previous IPL, for two reasons:
     *  - To allow in further signals that have not been masked while this
     *    handler is executing.
     *  - In case we do not return here: again, it is legal to longjmp() out of
     *    a handler, and if that happens the IPL would not be restored. POSIX
     *    specifies that the previous signal mask should be manually restored
     *    from the ucontext if that happens, but we can't expect POSIX
     *    applications to restore the IPL.
     */
    uint32_t prev_ipl;
    status_t ret __sys_unused = kern_thread_set_ipl(THREAD_SET_IPL_ALWAYS, ctx->ipl, &prev_ipl);
    libsystem_assert(ret == STATUS_SUCCESS);
    libsystem_assert(prev_ipl > POSIX_SIGNAL_IPL);

    /* Just in case something changed between the signal being queued and us
     * getting here. */
    if (action.sa_handler != SIG_DFL && action.sa_handler != SIG_IGN) {
        if (action.sa_flags & SA_SIGINFO) {
            ucontext_t ucontext = {};
            // TODO: uc_stack/SA_ONSTACK would require us to run the
            // callback on the other stack...
            memcpy(&ucontext.uc_mcontext, &ctx->cpu, sizeof(ucontext.uc_mcontext));
            ucontext.uc_sigmask = prev_mask;

            action.sa_sigaction(info->si_signo, info, &ucontext);
        } else {
            action.sa_handler(info->si_signo);
        }
    }

    /* Restore previous IPL (in case caller loops again). */
    ret = kern_thread_set_ipl(THREAD_SET_IPL_ALWAYS, prev_ipl, NULL);
    libsystem_assert(ret == STATUS_SUCCESS);
}

/** Kernel object event callback for a signal being raised. */
static void signal_condition_callback(object_event_t *event, thread_context_t *ctx) {
    while (true) {
        /* IPL is already at POSIX_SIGNAL_IPL + 1. */
        core_mutex_lock(&posix_signal_lock, -1);

        core_connection_t *conn = posix_service_get();
        if (!conn) {
            core_mutex_unlock(&posix_signal_lock);
            return;
        }

        siginfo_t pending;
        bool success = get_pending_signal_request(conn, &pending);

        /* signo == 0 indicates that there are no more pending signals. */
        if (!success || pending.si_signo == 0) {
            posix_service_put();
            core_mutex_unlock(&posix_signal_lock);
            break;
        }

        /* Releases lock/service on return. */
        handle_signal(conn, &pending, ctx);
    }
}

/** Kernel exception handler. */
static void posix_exception_handler(exception_info_t *info, thread_context_t *ctx) {
    libsystem_assert(info->code < core_array_size(posix_exception_signals));
    libsystem_assert(posix_exception_signals[info->code] != 0);
    libsystem_assert(info->code < core_array_size(posix_exception_codes));

    /* Construct a siginfo_t for the exception. */
    siginfo_t signal;
    signal.si_signo = posix_exception_signals[info->code];
    signal.si_code  = posix_exception_codes[info->code];
    signal.si_addr  = info->addr;

    if (info->code == EXCEPTION_PAGE_ERROR) {
        signal.si_errno = libsystem_status_to_errno_val(info->status);
    } else {
        signal.si_errno = 0;
    }

    /* Use 0 to indicate kernel. */
    signal.si_pid = 0;
    signal.si_uid = 0;

    /* IPL is already at THREAD_IPL_EXCEPTION + 1. */
    core_mutex_lock(&posix_signal_lock, -1);

    core_connection_t *conn = posix_service_get();
    if (!conn) {
        core_mutex_unlock(&posix_signal_lock);
        return;
    }

    /* Releases lock/service on return. */
    handle_signal(conn, &signal, ctx);
}

static bool set_signal_action(core_connection_t *conn, int32_t num, uint32_t disposition, uint32_t flags) {
    status_t ret;

    /* If this has a handler, set up the signal condition if we have not yet
     * done so. */
    if (disposition == POSIX_SIGNAL_DISPOSITION_HANDLER &&
        posix_signal_condition == INVALID_HANDLE)
    {
        if (!get_signal_condition_request(conn, &posix_signal_condition))
            return false;

        object_event_t event = {};
        event.handle = posix_signal_condition;
        event.event  = CONDITION_EVENT_SET;
        event.flags  = OBJECT_EVENT_EDGE;

        ret = kern_object_callback(&event, signal_condition_callback, POSIX_SIGNAL_IPL);
        if (ret != STATUS_SUCCESS) {
            libsystem_log(CORE_LOG_ERROR, "failed to register signal callback: %" PRId32, ret);

            kern_handle_close(posix_signal_condition);
            posix_signal_condition = INVALID_HANDLE;

            libsystem_status_to_errno(ret);
            return false;
        }
    }

    if (!set_signal_action_request(conn, num, disposition, flags))
        return false;

    /* If this signal maps to any exceptions, install/remove handlers as
     * necessary. TODO: Possibly should warn if another handler is already
     * installed as it means the app is mixing POSIX and native exception
     * handling. */
    for (size_t i = 0; i < core_array_size(posix_exception_signals); i++) {
        if (posix_exception_signals[i] == num) {
            if (disposition == POSIX_SIGNAL_DISPOSITION_HANDLER) {
                if (!(posix_exceptions_installed & (1 << i))) {
                    ret = kern_process_set_exception_handler(i, posix_exception_handler);
                    libsystem_assert(ret == STATUS_SUCCESS);

                    posix_exceptions_installed |= (1 << i);
                }
            } else {
                if (posix_exceptions_installed & (1 << i)) {
                    ret = kern_process_set_exception_handler(i, NULL);
                    libsystem_assert(ret == STATUS_SUCCESS);

                    posix_exceptions_installed &= ~(1 << i);
                }
            }
        }
    }

    return true;
}

/** Reset signal state after a fork. */
void posix_signal_fork(void) {
    SCOPED_SIGNAL_LOCK();

    /* Signal condition is not marked as inheritable. */
    posix_signal_condition = INVALID_HANDLE;

    /* If we have any non-default state, set this at the service. */
    core_connection_t *conn = NULL;

    for (int32_t i = 1; i < NSIG; i++) {
        posix_signal_t *signal = &posix_signals[i];

        if (signal->disposition != POSIX_SIGNAL_DISPOSITION_DEFAULT) {
            if (!conn)
                conn = posix_service_get();

            /* Note that kernel exception handlers are inherited so this won't
             * need to touch them, leave posix_exceptions_installed as is. */
            bool success = conn && set_signal_action(conn, i, signal->disposition, signal->action.sa_flags);

            if (!success) {
                libsystem_log(CORE_LOG_ERROR, "failed to set signal %" PRId32 " action after fork, resetting handler", i);
                memset(signal, 0, sizeof(*signal));
            }
        }
    }

    if (posix_signal_mask != 0) {
        if (!conn)
            conn = posix_service_get();

        bool success = conn && set_signal_mask_request(conn, posix_signal_mask);

        if (!success) {
            libsystem_log(CORE_LOG_ERROR, "failed to set signal mask after fork");
            posix_signal_mask = 0;
        }
    }

    if (conn)
        posix_service_put();
}

/**
 * Enter a region which should be guarded against signals. This raises the
 * current thread's IPL to (POSIX_SIGNAL_IPL + 1). This is necessary around
 * regions which take locks that signal handlers will need to take, to prevent
 * deadlock if a signal occurs while those locks are held.
 *
 * This is reference counted to handle nested calls.
 */
void posix_signal_guard_begin(void) {
    if (posix_signal_guard.count++ == 0) {
        status_t ret __sys_unused =
            kern_thread_set_ipl(THREAD_SET_IPL_RAISE, POSIX_SIGNAL_IPL + 1, &posix_signal_guard.prev_ipl);
        libsystem_assert(ret == STATUS_SUCCESS);
    }
}

/** Exit a region guarded against signals. */
void posix_signal_guard_end(void) {
    libsystem_assert(posix_signal_guard.count > 0);
    if (--posix_signal_guard.count == 0) {
        status_t ret __sys_unused = kern_thread_set_ipl(THREAD_SET_IPL_ALWAYS, posix_signal_guard.prev_ipl, NULL);
        libsystem_assert(ret == STATUS_SUCCESS);
    }
}

/** Convert a kernel exception code to a signal number. */
int posix_signal_from_exception(unsigned code) {
    if (code >= core_array_size(posix_exception_signals) || posix_exception_signals[code] == 0) {
        libsystem_log(CORE_LOG_WARN, "unhandled exception code %u", code);
        return SIGKILL;
    }

    return posix_exception_signals[code];
}

/**
 * Public API functions.
 */

/** Sends a signal to a process.
 * @param pid           ID of process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int kill(pid_t pid, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    bool success = kill_request(conn, pid, num);
    posix_service_put();
    return (success) ? 0 : -1;
}

/** Sends a signal to the current process.
 * @param num           Signal number.
 * @return              0 on success, -1 on failure. */
int raise(int num) {
    // TODO: Don't reach out to the POSIX service, handle internally.
    // Need to change IPL though.
    // Go to the service if currently masked.
    __asm__ volatile("ud2a");
    libsystem_stub("raise", true);
    return -1;
}

/** Examines or changes the action of a signal.
 * @param num           Signal number to modify.
 * @param act           Pointer to new action for signal (can be NULL).
 * @param old_act       Pointer to location to store previous action in (can
 *                      be NULL).
 * @return              0 on success, -1 on failure. */
int sigaction(int num, const sigaction_t *restrict act, sigaction_t *restrict old_act) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    SCOPED_SIGNAL_LOCK();

    posix_signal_t *signal = &posix_signals[num];

    if (old_act)
        memcpy(old_act, &signal->action, sizeof(*old_act));

    if (act) {
        uint32_t disposition;
        if (act->sa_handler == SIG_DFL) {
            disposition = POSIX_SIGNAL_DISPOSITION_DEFAULT;
        } else if (act->sa_handler == SIG_IGN) {
            disposition = POSIX_SIGNAL_DISPOSITION_IGNORE;
        } else {
            disposition = POSIX_SIGNAL_DISPOSITION_HANDLER;
        }

        /* See if anything needs to be updated at the service. */
        if (disposition != signal->disposition || act->sa_flags != signal->action.sa_flags) {
            core_connection_t *conn = posix_service_get();
            if (!conn) {
                errno = EAGAIN;
                return -1;
            }

            bool success = set_signal_action(conn, num, disposition, act->sa_flags);
            posix_service_put();

            if (!success)
                return -1;
        }

        memcpy(&signal->action, act, sizeof(*act));
        signal->disposition = disposition;
    }

    return 0;
}

/** Sets the handler of a signal.
 * @param num           Signal number.
 * @param handler       Handler function.
 * @return              Previous handler, or SIG_ERR on failure. */
sighandler_t signal(int num, sighandler_t handler) {
    sigaction_t act;
    act.sa_handler = handler;

    sigaction_t old_act;
    int ret = sigaction(num, &act, &old_act);
    if (ret != 0)
        return SIG_ERR;

    return old_act.sa_handler;
}

/** Sets the signal mask.
 * @param how           How to set the mask.
 * @param set           Signal set to mask (can be NULL).
 * @param old_set       Where to store previous masked signal set (can be NULL).
 * @return              0 on success, -1 on failure. */
int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict old_set) {
    SCOPED_SIGNAL_LOCK();

    if (old_set) {
        *old_set = posix_signal_mask;
    }

    if (set) {
        sigset_t val  = make_valid_sigmask(*set);
        sigset_t mask = posix_signal_mask;

        switch (how) {
            case SIG_BLOCK:
                mask |= val;
                break;
            case SIG_UNBLOCK:
                mask &= ~val;
                break;
            case SIG_SETMASK:
                mask = val;
                break;
            default:
                errno = EINVAL;
                return -1;
        }

        if (mask != posix_signal_mask) {
            core_connection_t *conn = posix_service_get();
            if (!conn) {
                errno = EAGAIN;
                return -1;
            }

            bool success = set_signal_mask_request(conn, mask);
            posix_service_put();

            if (!success)
                return -1;

            posix_signal_mask = mask;
        }
    }

    return 0;
}

/**
 * Gets and sets the alternate signal stack for the current thread. This stack
 * is used to execute signal handlers with the SA_ONSTACK flag set. The
 * alternate stack is a per-thread attribute. If fork() is called, the new
 * process' initial thread inherits the alternate stack from the thread that
 * called fork().
 *
 * @param ss            Alternate stack to set (can be NULL).
 * @param oset          Where to store previous alternate stack (can be NULL).
 *
 * @return              0 on success, -1 on failure.
 */
int sigaltstack(const stack_t *restrict ss, stack_t *restrict old_ss) {
    libsystem_stub("sigaltstack", false);
    return -1;
}

int sigsuspend(const sigset_t *mask) {
    libsystem_stub("sigsuspend", true);
    return -1;
}

/**
 * Saves the current execution environment to be restored by a call to
 * siglongjmp(). If specified, the current signal mask will also be saved.
 *
 * @param env           Buffer to save to.
 * @param savemask      If not 0, the current signal mask will be saved.
 *
 * @return              0 if returning from direct invocation, non-zero if
 *                      returning from siglongjmp().
 */
int sigsetjmp(sigjmp_buf env, int savemask) {
    libsystem_stub("sigsetjmp", false);

    //if (savemask)
    //  sigprocmask(SIG_BLOCK, NULL, &env->mask);

    //env->restore_mask = savemask;
    return setjmp(env->buf);
}

/**
 * Restores an execution environment saved by a previous call to sigsetjmp().
 * If the original call to sigsetjmp() specified savemask as non-zero, the
 * signal mask at the time of the call will be restored.
 *
 * @param env           Buffer to restore.
 * @param val           Value that the original sigsetjmp() call should return.
 */
void siglongjmp(sigjmp_buf env, int val) {
    libsystem_stub("siglongjmp", false);

    //if (env->restore_mask)
    //  sigprocmask(SIG_SETMASK, &env->mask, NULL);

    longjmp(env->buf, val);
}

/** Adds a signal to a signal set.
 * @param set           Set to add to.
 * @param num           Signal to add.
 * @return              0 on success, -1 on failure. */
int sigaddset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set |= (1 << num);
    return 0;
}

/** Removes a signal from a signal set.
 * @param set           Set to remove from.
 * @param num           Signal to remove.
 * @return              0 on success, -1 on failure. */
int sigdelset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~(1 << num);
    return 0;
}

/** Clears all signals in a signal set.
 * @param set           Set to clear.
 * @return              Always 0. */
int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

/** Sets all signals in a signal set.
 * @param set           Set to fill.
 * @return              Always 0. */
int sigfillset(sigset_t *set) {
    *set = (1 << NSIG) - 1;
    return 0;
}

/** Checks if a signal is included in a set.
 * @param set           Set to check.
 * @param num           Signal number to check for.
 * @return              1 if member, 0 if not, -1 if signal number is invalid. */
int sigismember(const sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    return (*set & (1 << num)) ? 1 : 0;
}

/** Array of signal strings. */
const char *const sys_siglist[NSIG] = {
    [SIGHUP]   = "Hangup",
    [SIGINT]   = "Interrupt",
    [SIGQUIT]  = "Quit",
    [SIGILL]   = "Illegal instruction",
    [SIGTRAP]  = "Trace trap",
    [SIGABRT]  = "Aborted",
    [SIGBUS]   = "Bus error",
    [SIGFPE]   = "Floating-point exception",
    [SIGKILL]  = "Killed",
    [SIGCHLD]  = "Child death/stop",
    [SIGSEGV]  = "Segmentation fault",
    [SIGSTOP]  = "Stopped",
    [SIGPIPE]  = "Broken pipe",
    [SIGALRM]  = "Alarm call",
    [SIGTERM]  = "Terminated",
    [SIGUSR1]  = "User signal 1",
    [SIGUSR2]  = "User signal 2",
    [SIGCONT]  = "Continued",
    [SIGURG]   = "Urgent I/O condition",
    [SIGTSTP]  = "Stopped (terminal)",
    [SIGTTIN]  = "Stopped (terminal input)",
    [SIGTTOU]  = "Stopped (terminal output)",
    [SIGWINCH] = "Window changed",
};

/** Gets the string representation of a signal number.
 * @return              Pointer to string. */
char *strsignal(int sig) {
    if (sig < 1 || sig >= NSIG)
        return (char *)"Unknown signal";

    return (char *)sys_siglist[sig];
}

/**
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param sig           Signal number to print.
 * @param s             Optional message to precede signal with.
 */
void psignal(int sig, const char *s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: %s\n", s, strsignal(sig));
    } else {
        fprintf(stderr, "%s\n", strsignal(sig));
    }
}

/**
 * Display a message on standard error followed by a string representation
 * of a signal.
 *
 * @param info          Signal to print information on.
 * @param s             Optional message to precede signal with.
 */
void psiginfo(const siginfo_t *info, const char *s) {
    psignal(info->si_signo, s);
}
