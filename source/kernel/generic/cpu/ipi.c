/* Kiwi inter-CPU communication functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Inter-CPU communication functions.
 */

#include <arch/ipi.h>
#include <arch/spinlock.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <mm/malloc.h>

#include <types/refcount.h>

#include <assert.h>
#include <errors.h>

extern void ipi_process_pending(void);

/** Number of message structures to allocate per CPU. */
#define IPI_MESSAGES_PER_CPU	4

/** IPI message information structure. */
typedef struct ipi_message {
	list_t header;			/**< Link to free list (also used by sender to track message). */
	list_t cpu_link;		/**< Link to destination CPU's message queue. */

	ipi_handler_t handler;		/**< Handler function for IPI. */
	unative_t data1;		/**< First handler argument. */
	unative_t data2;		/**< Second handler argument. */
	unative_t data3;		/**< Third handler argument. */
	unative_t data4;		/**< Fourth handler argument. */

	atomic_t acked;			/**< Whether the message has been acknowledged. */
	int status;			/**< Status code to return to sender. */
	refcount_t count;		/**< Reference count to track stuffs. */
} ipi_message_t;

/** List of free IPI message structures. */
static LIST_DECLARE(ipi_message_pool);
static SPINLOCK_DECLARE(ipi_message_lock);
static atomic_t ipi_message_count;

/* Whether IPI system is enabled. */
static bool ipi_enabled = false;

/** Get a free IPI message structure.
 * @return		IPI message structure. */
static ipi_message_t *ipi_message_get(void) {
	ipi_message_t *message = NULL;

	/* Wait for a message to become available. */
	while(message == NULL) {
		while(unlikely(atomic_get(&ipi_message_count) == 0)) {
			ipi_process_pending();
		}

		spinlock_lock(&ipi_message_lock, 0);

		/* Another CPU could have taken a message while we were waiting.
		 * If this is the case, go and retry. */
		if(atomic_get(&ipi_message_count) == 0) {
			spinlock_unlock(&ipi_message_lock);
			continue;
		}

		assert(!list_empty(&ipi_message_pool));

		/* Take the message structure off the list. */
		message = list_entry(ipi_message_pool.next, ipi_message_t, header);
		list_remove(&message->header);
		atomic_dec(&ipi_message_count);

		spinlock_unlock(&ipi_message_lock);
	}

	/* Initialize the structure. The reference count is set to 2 to
	 * account for both the destination and the sender. */
	atomic_set(&message->acked, 0);
	refcount_set(&message->count, 2);
	message->status = 0;

	return message;
}

/** Free an IPI message structure.
 * @param message	Message to free. */
static void ipi_message_release(ipi_message_t *message) {
	if(refcount_dec(&message->count) > 0) {
		return;
	}

	assert(list_empty(&message->cpu_link));

	spinlock_lock(&ipi_message_lock, 0);
	list_append(&ipi_message_pool, &message->header);
	atomic_inc(&ipi_message_count);
	spinlock_unlock(&ipi_message_lock);
}

/** Queue an IPI to a CPU and send an IPI if required.
 * @param message	Message to queue.
 * @param cpu		CPU to queue in. */
static void ipi_message_queue(ipi_message_t *message, cpu_t *cpu) {
	spinlock_lock(&cpu->ipi_lock, 0);

	list_append(&cpu->ipi_queue, &message->cpu_link);

	/* We don't send the CPU an IPI if it's already been sent an IPI that
	 * it hasn't finished handling yet to improve performance a little bit.
	 * The locking interaction with ipi_process_pending() ensures that this
	 * message will get processed even if we do not send an IPI. */
	if(!cpu->ipi_sent) {
		cpu->ipi_sent = true;
		ipi_send_interrupt(cpu->id);
	}

	spinlock_unlock(&cpu->ipi_lock);
}

/** Process pending IPI messages to the current CPU. */
void ipi_process_pending(void) {
	ipi_message_t *message;
	int ret;

	assert(ipi_enabled);

	spinlock_lock(&curr_cpu->ipi_lock, 0);

	/* If we're being called while spinning in ipi_message_get() or
	 * ipi_send(), then there may not have been an IPI sent. */
	if(!curr_cpu->ipi_sent) {
		spinlock_unlock(&curr_cpu->ipi_lock);
		return;
	}

	/* Loop through and handle each IPI that's been sent to us. */
	while(!list_empty(&curr_cpu->ipi_queue)) {
		message = list_entry(curr_cpu->ipi_queue.next, ipi_message_t, cpu_link);
		list_remove(&message->cpu_link);

		/* Unlock the queue while we call the handler. */
		spinlock_unlock(&curr_cpu->ipi_lock);

		/* Call the handler itself. */
		ret = message->handler(message, message->data1, message->data2,
		                       message->data3, message->data4);

		/* If the handler has not already been acknowledged, then
		 * acknowledge it now. */
		if(atomic_get(&message->acked) == 0) {
			message->status = ret;
			atomic_inc(&message->acked);
		}

		/* Release the message structure. */
		ipi_message_release(message);

		/* Relock the queue before we check again. */
		spinlock_lock(&curr_cpu->ipi_lock, 0);
	}

	curr_cpu->ipi_sent = false;
	spinlock_unlock(&curr_cpu->ipi_lock);
}

/** Send an IPI to a single CPU.
 *
 * Sends an IPI to a single CPU. This IPI will cause the specified handler to
 * be invoked on that CPU, with the four data parameters as arguments to it.
 * If the IPI_SEND_SYNC flag is specified, then this function will not return
 * until the function calls ipi_acknowledge() on the message. Otherwise, it
 * will return immediately after sending the message.
 *
 * @param dest		Destination CPU ID.
 * @param handler	Handler function for the message.
 * @param data1		First handler argument.
 * @param data2		Second handler argument.
 * @param data3		Third handler argument.
 * @param data4		Fourth handler argument.
 * @param flags		Behaviour flags.
 *
 * @return		The only error code that this function can return is
 *			ERR_NOT_FOUND. Otherwise, the return value depends
 *			on the behaviour flags: if IPI_SEND_SYNC was specified,
 *			then the return value is the return value passed to
 *			ipi_acknowledge(). Otherwise, it is 0.
 */
int ipi_send(cpu_id_t dest, ipi_handler_t handler, unative_t data1, unative_t data2,
             unative_t data3, unative_t data4, int flags) {
	bool state = intr_disable();
	ipi_message_t *message;
	int ret;

	/* Don't do anything if the IPI system isn't enabled. */
	if(!ipi_enabled) {
		intr_restore(state);
		return 0;
	}

	/* Check if the destination exists. */
	if(dest > cpu_id_max || !cpus[dest] || cpus[dest]->state == CPU_DOWN) {
		intr_restore(state);
		return -ERR_NOT_FOUND;
	}

	/* Get a message structure for the message. */
	message = ipi_message_get();
	message->handler = handler;
	message->data1 = data1;
	message->data2 = data2;
	message->data3 = data3;
	message->data4 = data4;

	/* Queue the message in the CPU's message queue and send it an IPI. */
	ipi_message_queue(message, cpus[dest]);

	if(flags & IPI_SEND_SYNC) {
		/* Synchronous, wait for the message to be acknowledged. */
		while(atomic_get(&message->acked) == 0) {
			ipi_process_pending();
		}

		ret = message->status;
		ipi_message_release(message);
		intr_restore(state);
		return ret;
	} else {
		/* Asynchronous, drop count on the message and return. */
		ipi_message_release(message);
		intr_restore(state);
		return 0;
	}
}

/** Send an IPI to all CPUs.
 *
 * Sends an IPI to all CPUs other than the calling CPU.. This IPI will cause
 * the specified handler to be invoked on those CPUs, with the four data
 * parameters as arguments to it. If the IPI_SEND_SYNC flag is specified, then
 * this function will not return until until the function has called
 * ipi_acknowledge() from every destination CPU on the message. Otherwise, it
 * will return immediately after sending the message.
 *
 * @note		Because this function can send to many destinations, it
 *			is not possible for a return value to propagate back
 *			from the CPUs to this function. If this is required,
 *			then ipi_send() should be used to individually send an
 *			IPI to each CPU in the cpus_running list.
 *
 * @param handler	Handler function for the message.
 * @param data1		First handler argument.
 * @param data2		Second handler argument.
 * @param data3		Third handler argument.
 * @param data4		Fourth handler argument.
 * @param flags		Behaviour flags.
 */
void ipi_broadcast(ipi_handler_t handler, unative_t data1, unative_t data2,
                   unative_t data3, unative_t data4, int flags) {
	bool state = intr_disable();
	LIST_DECLARE(sent_list);
	ipi_message_t *message;
	cpu_t *cpu;

	/* Don't do anything if the IPI system isn't enabled. */
	if(!ipi_enabled) {
		intr_restore(state);
		return;
	}

	/* Loop through all running CPUs, excluding ourselves. */
	LIST_FOREACH(&cpus_running, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu == curr_cpu) {
			continue;
		}

		/* Get a message structure to send to the CPU. */
		message = ipi_message_get();
		message->handler = handler;
		message->data1 = data1;
		message->data2 = data2;
		message->data3 = data3;
		message->data4 = data4;

		/* Queue the message in the CPU's message queue and send it an IPI. */
		ipi_message_queue(message, cpu);

		/* If we're sending synchronously, record the message.
		 * Otherwise drop the count and do no more with it. */
		if(flags & IPI_SEND_SYNC) {
			list_append(&sent_list, &message->header);
		} else {
			ipi_message_release(message);
		}
	}

	/* If sending synchronously, wait for all the sent messages to be
	 * acknowledged. */
	if(flags & IPI_SEND_SYNC) {
		do {
			LIST_FOREACH_SAFE(&sent_list, iter) {
				message = list_entry(iter, ipi_message_t, header);

				if(atomic_get(&message->acked) == 0) {
					continue;
				}

				list_remove(&message->header);
				ipi_message_release(message);
			}

			ipi_process_pending();
		} while(!list_empty(&sent_list));
	}

	intr_restore(state);
}

/** Acknowledge a message.
 *
 * Acknowledges an IPI message and sets its return code to the given value.
 * This function is only of use when the IPI is sent synchronously, and the
 * handler needs to acknowledge the message early. If this function is not
 * called by the handler function, then the message is instead acknowledged
 * immediately after its return, with the function's return value as the
 * status code.
 *
 * If the IPI was sent via ipi_broadcast(), then the status code specified to
 * this function will not be propagated back to the sender, as it would be
 * difficult to store the status code given by each CPU that received the
 * message.
 *
 * Finally, if the IPI was sent asynchronously, this function is of no use,
 * as the sender will have already continued what it was doing, and will have
 * no need for (nor be able to receive) a status code.
 *
 * @param ptr		Pointer to message being acknowledged (first argument
 *			to handler function).
 * @param status	Status code to return to sender.
 */
void ipi_acknowledge(void *ptr, int status) {
	ipi_message_t *message = ptr;

	message->status = status;
	atomic_inc(&message->acked);
}

/** Initialize the IPI message pool.  */
void __init_text ipi_init(void) {
	size_t i, count = cpu_count * IPI_MESSAGES_PER_CPU;
	ipi_message_t *messages;

	/* Don't need IPI support if we only have 1 CPU. */
	if(cpu_count == 1) {
		return;
	}

	/* Allocate message structures based on the total CPU count. */
	messages = kcalloc(count, sizeof(ipi_message_t), MM_FATAL);

	/* Initialize each structure and add it to the pool. */
	for(i = 0; i < count; i++) {
		list_init(&messages[i].header);
		list_init(&messages[i].cpu_link);

		list_append(&ipi_message_pool, &messages[i].header);
		atomic_inc(&ipi_message_count);
	}

	ipi_enabled = true;
}
