/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		Symmetric Multi-Processing (SMP) support.
 */

#include <lib/refcount.h>

#include <mm/malloc.h>

#include <assert.h>
#include <kboot.h>
#include <smp.h>
#include <status.h>

/** Boot option to disable SMP. */
KBOOT_BOOLEAN_OPTION("smp_disabled", "Disable SMP", false);

/** Number of call structures to allocate per CPU. */
#define SMP_CALLS_PER_CPU	4

/** SMP call information structure. */
typedef struct smp_call {
	struct smp_call *next;		/**< Next structure in the free pool. */
	list_t cpu_link;		/**< Link to destination CPU's call queue. */

	smp_call_func_t func;		/**< Handler function. */
	void *arg;			/**< Argument to handler. */

	atomic_t *result;		/**< Caller's completion count pointer. */
	status_t status;		/**< Function return status code. */
	refcount_t count;		/**< Reference count to track structure usage. */
} smp_call_t;

/** List of free call structures. */
static smp_call_t *smp_call_pool = NULL;
static SPINLOCK_DECLARE(smp_call_lock);

/** Whether SMP call system is enabled. */
static bool smp_call_enabled = false;

/** Variable used to synchronise the stages of the SMP boot process. */
volatile unsigned smp_boot_status = 0;

/** Get a free SMP call structure.
 * @return		SMP call structure. */
static smp_call_t *smp_call_get(void) {
	smp_call_t *call = NULL;

	/* Wait for a structure to become available. */
	while(!call) {
		/* While we are spinning for a structure to come available, a
		 * call may be made to this CPU. We must therefore handle
		 * incoming calls in this loop to ensure that call structures
		 * queued to us get freed up. */
		while(!smp_call_pool)
			smp_ipi_handler();

		spinlock_lock(&smp_call_lock);

		/* Another CPU could have taken a structure while we were
		 * acquiring the lock. If this is the case, go and retry. */
		if(!smp_call_pool) {
			spinlock_unlock(&smp_call_lock);
			continue;
		}

		/* Take the structure off the list. */
		call = smp_call_pool;
		smp_call_pool = call->next;
		spinlock_unlock(&smp_call_lock);
	}

	/* Set the reference count to 2 to account for both the destination and
	 * the caller. */
	refcount_set(&call->count, 2);
	return call;
}

/** Free an SMP call structure.
 * @param call		Call structure to free. */
static void smp_call_release(smp_call_t *call) {
	if(refcount_dec(&call->count) == 0) {
		assert(list_empty(&call->cpu_link));

		spinlock_lock(&smp_call_lock);
		call->next = smp_call_pool;
		smp_call_pool = call;
		spinlock_unlock(&smp_call_lock);
	}
}

/** Process pending calls to the current CPU. */
void smp_ipi_handler(void) {
	smp_call_t *call;
	status_t ret;

	assert(smp_call_enabled);

	spinlock_lock(&curr_cpu->call_lock);

	/* If we're being called while spinning in smp_call_get() or
	 * smp_call_send_*(), then there may not have been an IPI sent. */
	if(!curr_cpu->ipi_sent) {
		spinlock_unlock(&curr_cpu->call_lock);
		return;
	}

	/* Loop through and handle each call that's been queued to us. */
	while(!list_empty(&curr_cpu->call_queue)) {
		call = list_first(&curr_cpu->call_queue, smp_call_t, cpu_link);
		list_remove(&call->cpu_link);
		spinlock_unlock(&curr_cpu->call_lock);

		curr_cpu->curr_call = call;

		if(call->func) {
			ret = call->func(call->arg);
		} else {
			ret = STATUS_SUCCESS;
		}

		curr_cpu->curr_call = NULL;

		/* If the result pointer is NULL, either this is an asynchronous
		 * call and does not require acknowledgement, or the handler
		 * called smp_call_acknowledge(). Otherwise, the call must still
		 * be acknowledged. */
		if(call->result) {
			call->status = ret;
			atomic_dec(call->result);
		}

		/* Release the call structure. */
		smp_call_release(call);

		spinlock_lock(&curr_cpu->call_lock);
	}

	curr_cpu->ipi_sent = false;
	spinlock_unlock(&curr_cpu->call_lock);
}

/** Queue a call to a CPU and send an IPI if required.
 * @param call		Call to queue.
 * @param cpu		CPU to queue in. */
static void smp_call_queue(smp_call_t *call, cpu_t *cpu) {
	spinlock_lock(&cpu->call_lock);

	list_append(&cpu->call_queue, &call->cpu_link);

	/* Avoid sending the CPU an IPI again if it has already been sent one
	 * that it hasn't finished handling yet. The locking interaction with
	 * smp_ipi_handler() ensures that this message will get processed even
	 * if we do not send an IPI. */
	if(!cpu->ipi_sent) {
		cpu->ipi_sent = true;
		arch_smp_ipi(cpu->id);
	}

	spinlock_unlock(&cpu->call_lock);
}

/**
 * Call a function on a single CPU.
 *
 * Interrupts a single CPU and causes the specified function to be called on it.
 * If the SMP_CALL_ASYNC flag is specified, this function will return
 * immediately after queueing the call. Otherwise, it will not return until the
 * called function returns, or it calls smp_call_acknowledge(). The function
 * pointer can be specified as NULL, which has the effect of prodding the CPU
 * to reschedule if it is idle.
 *
 * If the CPU specified is the current CPU, the function will be called
 * directly, and this function will not return until it has completed.
 *
 * @param dest		Destination CPU ID (must exist).
 * @param func		Function to call (can be NULL, as explained above).
 * @param arg		Argument to pass to the function.
 * @param flags		Behaviour flags.
 *
 * @return		The status code returned depends on the behaviour
 *			flags: if SMP_CALL_ASYNC was specified, it is always
 *			STATUS_SUCCESS. Otherwise, it is the value returned by
 *			the called function or set with smp_call_acknowledge().
 */
status_t smp_call_single(cpu_id_t dest, smp_call_func_t func, void *arg, unsigned flags) {
	smp_call_t *call;
	atomic_t acked;
	status_t ret;
	bool state;

	state = local_irq_disable();

	if(dest == curr_cpu->id) {
		ret = func(arg);
		local_irq_restore(state);
		return ret;
	}

	/* Don't do anything more if the call system isn't enabled, other CPUs
	 * won't be up. */
	if(!smp_call_enabled) {
		local_irq_restore(state);
		return STATUS_SUCCESS;
	}

	/* Destination must exist. */
	if(dest > highest_cpu_id || !cpus[dest])
		fatal("Attempting to call on non-existant CPU");

	call = smp_call_get();
	call->func = func;
	call->arg = arg;

	/* Only 1 CPU to acknowledge. */
	if(!(flags & SMP_CALL_ASYNC)) {
		acked = 1;
		call->result = &acked;
	} else {
		call->result = NULL;
	}

	/* Queue the call in the CPU's call queue and send it an IPI. */
	smp_call_queue(call, cpus[dest]);

	if(flags & SMP_CALL_ASYNC) {
		/* Asynchronous, just drop the count and return. */
		ret = STATUS_SUCCESS;
	} else {
		/* Synchronous, wait for the message to be acknowledged. */
		while(atomic_get(&acked) != 0)
			smp_ipi_handler();

		ret = call->status;
	}

	smp_call_release(call);
	local_irq_restore(state);
	return ret;
}

/**
 * Call a function on all remote CPUs.
 *
 * Interrupts all remote CPUs and causes the specified function to be called
 * on them. If the SMP_CALL_ASYNC flag is specified, this function will
 * return immediately after queueing the call. Otherwise, it will not return
 * until the called function returns or calls smp_call_acknowledge() on all
 * remote CPUs.
 *
 * The return values of the called function is not propagated back to the
 * calling CPU, as it is not possible to handle the return value from all
 * target CPUs.
 *
 * @param func		Function to call (must not be NULL).
 * @param arg		Argument to pass to the function.
 * @param flags		Behaviour flags.
 */
void smp_call_broadcast(smp_call_func_t func, void *arg, unsigned flags) {
	atomic_t acked = 0;
	smp_call_t *call;
	cpu_t *cpu;
	bool state;

	state = local_irq_disable();

	/* Don't do anything if the call system isn't enabled. */
	if(!smp_call_enabled) {
		local_irq_restore(state);
		return;
	}

	/* Loop through all running CPUs, excluding ourselves. */
	LIST_FOREACH(&running_cpus, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu == curr_cpu)
			continue;

		call = smp_call_get();
		call->func = func;
		call->arg = arg;

		if(!(flags & SMP_CALL_ASYNC)) {
			atomic_inc(&acked);
			call->result = &acked;
		} else {
			call->result = NULL;
		}

		/* Queue the call in the CPU's call queue and send it an IPI. */
		smp_call_queue(call, cpu);

		/* We do not need to keep the structure around on this side as
		 * we have no status code to collect from it. */
		smp_call_release(call);
	}

	/* If calling synchronously, wait for all the sent messages to be
	 * acknowledged. */
	if(!(flags & SMP_CALL_ASYNC)) {
		while(atomic_get(&acked) != 0)
			smp_ipi_handler();
	}

	local_irq_restore(state);
}

/**
 * Acknowledge a call from another CPU.
 *
 * Acknowledges the call from another CPU that is currently being executed,
 * and sets its status code to the given value. This function is only of use
 * when the call is sent synchronously, and the handler needs to acknowledge
 * the call without returning. If this function is not called by the handler
 * function, then the call is instead acknowledged immediately after its
 * return, with the function's return value as the status code.
 *
 * If the call was sent as a multicast/broadcast, then the status code passed
 * to this function will not be propagated back to the sender, as described for
 * smp_call_broadcast().

 * @param status	Status code to return to sender.
 */
void smp_call_acknowledge(status_t status) {
	assert(curr_cpu->curr_call);

	if(curr_cpu->curr_call->result) {
		curr_cpu->curr_call->status = status;
		atomic_dec(curr_cpu->curr_call->result);
		curr_cpu->curr_call->result = NULL;
	}
}

/** Initialize the SMP call system and detect secondary CPUs. */
__init_text void smp_init(void) {
	smp_call_t *calls;
	size_t i, count;

	/* If SMP is forced to be disabled, do not need to do anything. */
	if(kboot_boolean_option("smp_disabled"))
		return;

	/* First we want to detect secondary CPUs. */
	platform_smp_detect();

	/* If we only have 1 CPU, there is no need to set up the call system. */
	if(cpu_count == 1)
		return;

	/* Allocate message structures based on the total CPU count. */
	count = cpu_count * SMP_CALLS_PER_CPU;
	calls = kcalloc(count, sizeof(smp_call_t), MM_BOOT);

	/* Initialize each structure and add it to the pool. */
	for(i = 0; i < count; i++) {
		list_init(&calls[i].cpu_link);

		calls[i].next = smp_call_pool;
		smp_call_pool = &calls[i];
	}

	smp_call_enabled = true;
}

/** Boot secondary CPUs. */
__init_text void smp_boot(void) {
	cpu_id_t i;
	bool state;

	state = local_irq_disable();
	platform_smp_boot_prepare();

	for(i = 0; i <= highest_cpu_id; i++) {
		if(cpus[i] && cpus[i]->state == CPU_OFFLINE) {
			smp_boot_status = SMP_BOOT_INIT;
			platform_smp_boot(cpus[i]);
		}
	}

	platform_smp_boot_cleanup();

	/* Indicate to the newly-booted CPUs that all CPUs are up and they
	 * can start scheduling threads (see kmain_secondary()). */
	smp_boot_status = SMP_BOOT_COMPLETE;

	local_irq_restore(state);
}
