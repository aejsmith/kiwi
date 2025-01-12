/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Input device class.
 *
 * TODO:
 *  - Proper handling for dropping events when the queue is full. This is
 *    necessary for where the client might be tracking state from events which
 *    give relative state (e.g. tracking button state). See how Linux libevdev
 *    handles this.
 */

#include <device/input/input.h>

#include <device/class.h>

#include <io/request.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <sync/condvar.h>

#include <assert.h>
#include <module.h>
#include <status.h>

#define INPUT_BUFFER_SIZE   128

/**
 * Client for an input device. Each open handle to an input device gets its
 * own event queue, incoming events will be duplicated out to each client.
 */
typedef struct input_client {
    list_t link;

    mutex_t lock;               /**< Input buffer lock. */
    condvar_t cvar;             /**< Condition to wait for input on. */
    size_t start;               /**< Start position in input buffer. */
    size_t size;                /**< Current size of input buffer. */
    notifier_t notifier;        /**< Data notifier. */

    /** Input event buffer. */
    input_event_t buffer[INPUT_BUFFER_SIZE];
} input_client_t;

static device_class_t input_device_class;

/** Cleans up all data associated with an input device. */
static void input_device_destroy_impl(device_t *_device) {
    input_device_t *device = _device->private;

    assert(list_empty(&device->clients));

    // TODO: Need destruction for derived...

    kfree(device);
}

/** Opens an input device. */
static status_t input_device_open(device_t *_device, uint32_t flags, void **_private) {
    input_device_t *device = _device->private;

    /* This is quite large so we use MM_USER. */
    input_client_t *client = kmalloc(sizeof(input_client_t), MM_USER);
    if (!client)
        return STATUS_NO_MEMORY;

    list_init(&client->link);
    mutex_init(&client->lock, "input_client_lock", 0);
    condvar_init(&client->cvar, "input_client_cvar");
    notifier_init(&client->notifier, NULL);

    client->start = 0;
    client->size  = 0;

    memset(client->buffer, 0, sizeof(client->buffer));

    mutex_lock(&device->clients_lock);
    list_append(&device->clients, &client->link);
    mutex_unlock(&device->clients_lock);

    *_private = client;
    return STATUS_SUCCESS;
}

/** Closes an input device. */
static void input_device_close(device_t *_device, file_handle_t *handle) {
    input_device_t *device = _device->private;
    input_client_t *client = handle->private;

    /* Shouldn't have anyone left waiting when we're being closed. */
    assert(notifier_empty(&client->notifier));

    mutex_lock(&device->clients_lock);
    list_remove(&client->link);
    mutex_unlock(&device->clients_lock);

    kfree(client);
}

/** Signals that an input device event is being waited for. */
static status_t input_device_wait(device_t *_device, file_handle_t *handle, object_event_t *event) {
    input_client_t *client = handle->private;

    if (event->event == FILE_EVENT_READABLE) {
        mutex_lock(&client->lock);

        if (client->size > 0) {
            object_event_signal(event, 0);
        } else {
            notifier_register(&client->notifier, object_event_notifier, event);
        }

        mutex_unlock(&client->lock);
        return STATUS_SUCCESS;
    } else {
        return STATUS_INVALID_EVENT;
    }
}

/** Stops waiting for an input device event. */
static void input_device_unwait(device_t *_device, file_handle_t *handle, object_event_t *event) {
    input_client_t *client = handle->private;

    if (event->event == FILE_EVENT_READABLE) {
        mutex_lock(&client->lock);
        notifier_unregister(&client->notifier, object_event_notifier, event);
        mutex_unlock(&client->lock);
    }
}

/** Performs I/O on an input device. */
static status_t input_device_io(device_t *_device, file_handle_t *handle, io_request_t *request) {
    input_client_t *client = handle->private;

    if (request->op != IO_OP_READ) {
        return STATUS_NOT_SUPPORTED;
    } else if (request->total % sizeof(input_event_t)) {
        return STATUS_INVALID_ARG;
    }

    mutex_lock(&client->lock);

    status_t ret = STATUS_SUCCESS;

    size_t count   = request->total / sizeof(input_event_t);
    for (size_t i = 0; i < count; i++) {
        if (client->size == 0) {
            if (request->flags & FILE_NONBLOCK) {
                ret = STATUS_WOULD_BLOCK;
                break;
            }

            ret = condvar_wait_cond_etc(
                &client->cvar, &client->lock, -1, SLEEP_INTERRUPTIBLE,
                client->size > 0);
            if (ret != STATUS_SUCCESS)
                break;
        }

        ret = io_request_copy(request, &client->buffer[client->start], sizeof(input_event_t), true);
        if (ret != STATUS_SUCCESS)
            break;

        client->size--;
        if (++client->start == INPUT_BUFFER_SIZE)
            client->start = 0;
    }

    mutex_unlock(&client->lock);
    return ret;
}

static const device_ops_t input_device_ops = {
    .type    = FILE_TYPE_CHAR,
    .destroy = input_device_destroy_impl,
    .open    = input_device_open,
    .close   = input_device_close,
    .wait    = input_device_wait,
    .unwait  = input_device_unwait,
    .io      = input_device_io,
};

/**
 * Adds an event to an input device's buffers. This function cannot be called
 * in interrupt context.
 *
 * @param device        Device to add to.
 * @param event         Event to add.
 */
__export void input_device_event(input_device_t *device, input_event_t *event) {
    mutex_lock(&device->clients_lock);

    list_foreach(&device->clients, iter) {
        input_client_t *client = list_entry(iter, input_client_t, link);

        mutex_lock(&client->lock);

        if (client->size < INPUT_BUFFER_SIZE) {
            size_t pos = (client->start + client->size) % INPUT_BUFFER_SIZE;
            client->size++;

            client->buffer[pos].time  = event->time;
            client->buffer[pos].type  = event->type;
            client->buffer[pos].value = event->value;

            condvar_signal(&client->cvar);
            notifier_run(&client->notifier, NULL, true);
        } else {
            /* TODO. */
            device_kprintf(device->node, LOG_WARN, "buffer full, dropping event\n");
        }

        mutex_unlock(&client->lock);
    }

    mutex_unlock(&device->clients_lock);
}

static status_t create_input_device(
    input_device_t *device, const char *name, device_t *parent,
    input_device_type_t type, module_t *module)
{
    mutex_init(&device->clients_lock, "input_device_clients_lock", 0);
    list_init(&device->clients);

    device->type = type;

    // TODO: Make it possible to set these later and then remove the type
    // parameter, just have the driver set the field like we do in other drivers.
    device_attr_t attrs[] = {
        { INPUT_DEVICE_ATTR_TYPE, DEVICE_ATTR_INT32, { .int32 = type } },
    };

    return device_class_create_device(
        &input_device_class, module_caller(), name, parent, &input_device_ops,
        device, attrs, array_size(attrs), 0, &device->node);
}

/**
 * Initializes a new input device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * input_device_publish().
 *
 * @param device        Device to initialize.
 * @param name          Name to give the device node.
 * @param parent        Parent device node.
 * @param type          Type of the device.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t input_device_create_etc(
    input_device_t *device, const char *name, device_t *parent,
    input_device_type_t type)
{
    module_t *module = module_caller();
    return create_input_device(device, name, parent, type, module);
}

/**
 * Initializes a new input device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * input_device_publish().
 *
 * The device will be named after the module creating the device.
 *
 * @param device        Device to initialize.
 * @param parent        Parent device node (e.g. bus device).
 * @param type          Type of the device.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t input_device_create(input_device_t *device, device_t *parent, input_device_type_t type) {
    module_t *module = module_caller();
    return create_input_device(device, module->name, parent, type, module);
}

/**
 * Publishes an input device. This completes initialization after the driver
 * has finished initialization, and then publishes the device for use.
 *
 * @param device        Device to publish.
 */
__export void input_device_publish(input_device_t *device) {
    device_publish(device->node);
}

static status_t input_init(void) {
    return device_class_init(&input_device_class, INPUT_DEVICE_CLASS_NAME);
}

static status_t input_unload(void) {
    return device_class_destroy(&input_device_class);
}

MODULE_NAME(INPUT_MODULE_NAME);
MODULE_DESC("Input device class manager");
MODULE_FUNCS(input_init, input_unload);
