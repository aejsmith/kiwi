/*
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
 * @brief		Unidirectional data pipe implementation.
 */

#include <ipc/pipe.h>

#include <mm/kheap.h>
#include <mm/malloc.h>

#include <proc/handle.h>

#include <assert.h>
#include <errors.h>

/** Read a byte from a pipe.
 * @param pipe		Pipe structure. Should be locked, and have data.
 * @return		Character read. */
static inline char pipe_get(pipe_t *pipe) {
	char ch;

	ch = pipe->buf[pipe->start];
	if(++pipe->start >= PIPE_SIZE) {
		pipe->start = 0;
	}

	semaphore_up(&pipe->space_sem, 1);
	return ch;
}

/** Write a byte to a pipe.
 * @param pipe		Pipe structure. Should be locked, and have space.
 * @param ch		Character to write. */
static inline void pipe_insert(pipe_t *pipe, char ch) {
	pipe->buf[pipe->end] = ch;
	if(++pipe->end >= PIPE_SIZE) {
		pipe->end = 0;
	}

	semaphore_up(&pipe->data_sem, 1);
}

/** Read data from a pipe.
 *
 * Reads data from a pipe into a buffer. Reads of less than or equal to
 * PIPE_SIZE will either read all the requested data, or none at all. Reads of
 * greater than PIPE_SIZE may only return part of the data. A read may not be
 * able to read all data if the calling thread is interrupted, or non-blocking
 * mode is requested.
 *
 * @param pipe		Pipe to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		0 on success, negative error code on failure.
 */
int pipe_read(pipe_t *pipe, char *buf, size_t count, bool nonblock, size_t *bytesp) {
	size_t i = 0;
	int ret;

	mutex_lock(&pipe->reader);

	if(count <= PIPE_SIZE) {
		/* Try to get all required data before reading. */
		for(i = 0; i < count; i++) {
			if((ret = semaphore_down_etc(&pipe->data_sem, (nonblock) ? 0 : -1,
			                             SYNC_INTERRUPTIBLE)) != 0) {
				semaphore_up(&pipe->data_sem, i);
				i = 0;
				goto out;
			}
		}

		mutex_lock(&pipe->lock);
		for(i = 0; i < count; i++) {
			buf[i] = pipe_get(pipe);
		}
		notifier_run(&pipe->space_notifier, NULL, false);
		mutex_unlock(&pipe->lock);
	} else {
		for(i = 0; i < count; i++) {
			if((ret = semaphore_down_etc(&pipe->data_sem, (nonblock) ? 0 : -1,
			                             SYNC_INTERRUPTIBLE)) != 0) {
				goto out;
			}

			mutex_lock(&pipe->lock);
			buf[i] = pipe_get(pipe);
			notifier_run(&pipe->space_notifier, NULL, false);
			mutex_unlock(&pipe->lock);
		}
	}

	ret = 0;
out:
	mutex_unlock(&pipe->reader);
	if(bytesp) {
		*bytesp = i;
	}
	return ret;
}

/** Write data to a pipe.
 *
 * Writes data from a buffer to a pipe. Writes of less than or equal to
 * PIPE_SIZE will either write all the requested data, or none at all. Writes
 * of greater than PIPE_SIZE may only write part of the data. A write may not
 * be able to write all data if the calling thread is interrupted, or
 * non-blocking mode is requested.
 *
 * @param pipe		Pipe to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes written.
 *
 * @return		0 on success, negative error code on failure.
 */
int pipe_write(pipe_t *pipe, const char *buf, size_t count, bool nonblock, size_t *bytesp) {
	size_t i = 0;
	int ret;

	mutex_lock(&pipe->writer);

	if(count <= PIPE_SIZE) {
		/* Try to get all required space before writing. */
		for(i = 0; i < count; i++) {
			if((ret = semaphore_down_etc(&pipe->space_sem, (nonblock) ? 0 : -1,
			                             SYNC_INTERRUPTIBLE)) != 0) {
				semaphore_up(&pipe->space_sem, i);
				i = 0;
				goto out;
			}
		}

		/* For atomic writes, we only run the data notifier after
		 * writing everything, so we don't do too many calls. */
		mutex_lock(&pipe->lock);
		for(i = 0; i < count; i++) {
			pipe_insert(pipe, buf[i]);
		}
		notifier_run(&pipe->data_notifier, NULL, false);
		mutex_unlock(&pipe->lock);
	} else {
		for(i = 0; i < count; i++) {
			if((ret = semaphore_down_etc(&pipe->space_sem, (nonblock) ? 0 : -1,
			                             SYNC_INTERRUPTIBLE)) != 0) {
				goto out;
			}

			mutex_lock(&pipe->lock);
			pipe_insert(pipe, buf[i]);
			notifier_run(&pipe->data_notifier, NULL, false);
			mutex_unlock(&pipe->lock);
		}
	}

	ret = 0;
out:
	mutex_unlock(&pipe->writer);
	if(bytesp) {
		*bytesp = i;
	}
	return ret;
}

/** Wait for a pipe to be readable or writable.
 *
 * Waits for a pipe to become readable or writable, and notifies the specified
 * handle_wait_t structure when it is. This is a convenience function, for
 * example for devices that use pipes internally.
 *
 * @param pipe		Pipe to wait for.
 * @param write		Whether to wait to be writable (pipe is classed as
 *			writable when there is space in the buffer).
 * @param wait		Wait structure to notify.
 */
void pipe_wait(pipe_t *pipe, bool write, handle_wait_t *wait) {
	if(write) {
		if(semaphore_count(&pipe->space_sem)) {
			wait->cb(wait);
		} else {
			notifier_register(&pipe->space_notifier, handle_wait_notifier, wait);
		}
	} else {
		if(semaphore_count(&pipe->data_sem)) {
			wait->cb(wait);
		} else {
			notifier_register(&pipe->data_notifier, handle_wait_notifier, wait);
		}
	}
}

/** Stop waiting for a pipe.
 *
 * Stops waiting for a pipe event.
 *
 * @param pipe		Pipe to stop waiting for.
 * @param write		Whether waiting to be writable.
 * @param wait		Wait structure.
 */
void pipe_unwait(pipe_t *pipe, bool write, handle_wait_t *wait) {
	notifier_unregister((write) ? &pipe->space_notifier : &pipe->data_notifier, handle_wait_notifier, wait);
}

/** Create a new pipe.
 *
 * Allocates a new pipe structure.
 *
 * @return		Pointer to pipe structure.
 */
pipe_t *pipe_create(void) {
	pipe_t *pipe = kmalloc(sizeof(pipe_t), MM_SLEEP);

	mutex_init(&pipe->reader, "pipe_reader_lock", 0);
	mutex_init(&pipe->writer, "pipe_writer_lock", 0);
	mutex_init(&pipe->lock, "pipe_lock", 0);
	semaphore_init(&pipe->space_sem, "pipe_space_sem", PIPE_SIZE);
	semaphore_init(&pipe->data_sem, "pipe_data_sem", 0);
	notifier_init(&pipe->space_notifier, pipe);
	notifier_init(&pipe->data_notifier, pipe);
	pipe->buf = kheap_alloc(PIPE_SIZE, MM_SLEEP);
	pipe->start = 0;
	pipe->end = 0;

	return pipe;
}

/** Destroy a pipe.
 *
 * Destroys a pipe structure previously created with pipe_create().
 *
 * @note		It is up to the caller to ensure that nothing will be
 *			using this pipe. For example, the handle system can be
 *			used for this purpose.
 *
 * @param pipe		Pipe to destroy.
 */
void pipe_destroy(pipe_t *pipe) {
	assert(!mutex_held(&pipe->reader));
	assert(!mutex_held(&pipe->writer));
	kfree(pipe->buf);
	kfree(pipe);
}
