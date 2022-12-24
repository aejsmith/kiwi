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
 * @brief               Disk device class.
 *
 * TODO:
 *  - UUID-based aliases for devices.
 */

#include <io/request.h>

#include <mm/malloc.h>

#include <lib/string.h>

#include <assert.h>
#include <module.h>
#include <status.h>

#include "disk.h"

device_class_t disk_device_class;

static void disk_device_destroy_impl(device_t *node) {
    disk_device_t *device = node->private;

    // TODO. Call device destroy function.
    (void)device;
    fatal("TODO");
}

static void disk_device_size(device_t *node, offset_t *_size, size_t *_block_size) {
    disk_device_t *device = node->private;

    *_size       = device->block_count * device->block_size;
    *_block_size = device->block_size;
}

/**
 * Allocates a block buffer suitable for transfers to/from the device. This uses
 * the device's optimal_block_size.
 */
static void *alloc_block_buffer(disk_device_t *device, dma_ptr_t *_dma) {
    if (device->flags & DISK_DEVICE_DMA) {
        /* TODO: Once we have a DMA pool allocator API we should use that for
         * buffer allocations: DMA constraints might not hit the allocation fast
         * path, and could also be smaller than a page. */
        size_t size = round_up(device->optimal_block_size, PAGE_SIZE);
        dma_alloc(device->node, size, &device->dma_constraints, MM_KERNEL, _dma);
        return dma_map(device->node, *_dma, size, MM_KERNEL);
    } else {
        *_dma = 0;
        return kmalloc(device->optimal_block_size, MM_KERNEL);
    }
}

static void free_block_buffer(disk_device_t *device, void *buf, dma_ptr_t dma) {
    if (device->flags & DISK_DEVICE_DMA) {
        size_t size = round_up(device->optimal_block_size, PAGE_SIZE);
        dma_unmap(buf, size);
        dma_free(device->node, dma, size);
    } else {
        kfree(buf);
    }
}

static status_t disk_device_io(device_t *node, file_handle_t *handle, io_request_t *request) {
    disk_device_t *device = node->private;
    status_t ret;

    /* Ensure that we do not go past the end of the device. */
    if ((uint64_t)request->offset >= device->size || !request->total)
        return STATUS_SUCCESS;

    if (request->op == IO_OP_READ && !device->ops->read_blocks) {
        return STATUS_NOT_SUPPORTED;
    } else if (request->op == IO_OP_WRITE && !device->ops->write_blocks) {
        return STATUS_NOT_SUPPORTED;
    }

    size_t total = ((uint64_t)(request->offset + request->total) > device->size)
        ? (size_t)(device->size - request->offset)
        : request->total;

    /* Work out the start and end blocks. Subtract one from count to prevent end
     * from going onto the next block when the offset plus the count is an exact
     * multiple of the block size. */
    uint64_t start = request->offset / device->block_size;
    uint64_t end   = (request->offset + (total - 1)) / device->block_size;

    void *block_buf     = NULL;
    dma_ptr_t block_dma = 0;

    /* If we're not starting on a block boundary, we need to do a partial
     * transfer on the initial block to get us up to a block boundary. If the
     * transfer only goes across one block, this will handle it. */
    uint32_t block_offset = request->offset % device->block_size;
    if (block_offset > 0) {
        block_buf = alloc_block_buffer(device, &block_dma);

        /* For a write, we need to partially update the current block contents,
         * so we need to read in the block regardless of the operation. */
        ret = device->ops->read_blocks(device, block_buf, block_dma, start, 1);
        if (ret != STATUS_SUCCESS)
            goto out;

        size_t size = (start != end)
            ? (size_t)(device->block_size - block_offset)
            : total;

        ret = io_request_copy(request, block_buf + block_offset, size, false);
        if (ret != STATUS_SUCCESS)
            goto out;

        /* If we're writing, write back the partially updated block. */
        if (request->op == IO_OP_WRITE) {
            ret = device->ops->write_blocks(device, block_buf, block_dma, start, 1);
            if (ret != STATUS_SUCCESS)
                goto out;
        }

        request->transferred += size;
        total -= size;
        start++;
    }

    /* Handle any full blocks. */
    while (total >= device->block_size) {
        /*
         * TODO: Use the request memory directly where possible. This needs an
         * io_request_map() equivalent for DMA. Should be able to check whether
         * memory satisfies constraints and allow direct mapping if so.
         *
         * This would also need some work to support multiple block transfers
         * in one go. The read/write_blocks APIs would need to accept a list of
         * transfers rather than just one contiguous range: DMA hardware can
         * typically handle a list of non-contiguous transfer regions for one
         * hardware request, so we should compile as many block transfers as
         * we can into one request to the driver.
         */

        /* Use optimal block size if we can - large enough transfer and aligned
         * to the optimal size. */
        uint32_t curr_block_size = device->block_size;
        uint32_t curr_blocks     = 1;
        if (total >= device->optimal_block_size && (start & (device->blocks_per_optimal_block - 1)) == 0) {
            curr_block_size = device->optimal_block_size;
            curr_blocks     = device->blocks_per_optimal_block;
        }

        if (!block_buf)
            block_buf = alloc_block_buffer(device, &block_dma);

        if (request->op == IO_OP_WRITE) {
            /* Get the data to write into the temporary buffer. */
            ret = io_request_copy(request, block_buf, curr_block_size, false);
            if (ret != STATUS_SUCCESS)
                goto out;
        }

        ret = (request->op == IO_OP_WRITE)
            ? device->ops->write_blocks(device, block_buf, block_dma, start, curr_blocks)
            : device->ops->read_blocks(device, block_buf, block_dma, start, curr_blocks);
        if (ret != STATUS_SUCCESS)
            goto out;

        if (request->op == IO_OP_READ) {
            /* Copy back from the temporary buffer. */
            ret = io_request_copy(request, block_buf, curr_block_size, false);
            if (ret != STATUS_SUCCESS)
                goto out;
        }

        request->transferred += curr_block_size;
        total -= curr_block_size;
        start += curr_blocks;
    }

    /* Handle anything that's left. This is similar to the first case. */
    if (total) {
        if (!block_buf)
            block_buf = alloc_block_buffer(device, &block_dma);

        /* As before, for a write, need to partial update. */
        ret = device->ops->read_blocks(device, block_buf, block_dma, start, 1);
        if (ret != STATUS_SUCCESS)
            goto out;

        ret = io_request_copy(request, block_buf, total, false);
        if (ret != STATUS_SUCCESS)
            goto out;

        if (request->op == IO_OP_WRITE) {
            ret = device->ops->write_blocks(device, block_buf, block_dma, start, 1);
            if (ret != STATUS_SUCCESS)
                goto out;
        }

        request->transferred += total;
    }

    ret = STATUS_SUCCESS;

out:
    if (block_buf)
        free_block_buffer(device, block_buf, block_dma);

    return ret;
}

const device_ops_t disk_device_ops = {
    .type    = FILE_TYPE_BLOCK,

    .destroy = disk_device_destroy_impl,
    .size    = disk_device_size,
    .io      = disk_device_io,
};

static status_t create_disk_device(
    disk_device_t *device, const char *name, device_t *parent, module_t *module)
{
    /* Keep this function in sync with add_partition(). */

    memset(device, 0, sizeof(*device));

    return device_class_create_device(
        &disk_device_class, module, name, parent, &disk_device_ops, device,
        NULL, 0, 0, &device->node);
}

/**
 * Initializes a new disk device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * disk_device_publish().
 *
 * @param device        Device to initialize.
 * @param name          Name to give the device node.
 * @param parent        Parent device node.
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t disk_device_create_etc(disk_device_t *device, const char *name, device_t *parent) {
    module_t *module = module_caller();
    return create_disk_device(device, name, parent, module);
}

/**
 * Initializes a new disk device. This only creates a device tree node and
 * initializes some state in the device, the device will not yet be used.
 * Once the driver has completed initialization, it should call
 * disk_device_publish().
 *
 * The device will be named after the module creating the device.
 *
 * @param device        Device to initialize.
 * @param parent        Parent device node (e.g. bus device).
 *
 * @return              Status code describing the result of the operation.
 */
__export status_t disk_device_create(disk_device_t *device, device_t *parent) {
    module_t *module = module_caller();
    return create_disk_device(device, module->name, parent, module);
}

/**
 * Publishes a disk device. This completes initialization after the driver
 * has finished initialization, and then publishes the device for use.
 *
 * @param device        Device to publish.
 */
__export void disk_device_publish(disk_device_t *device) {
    device->size = device->block_count * device->block_size;

    /* If physical block size is larger than the logical block size, that is
     * more optimal for I/O. Additionally, we set the optimal size to at least
     * PAGE_SIZE, because most disk access will be through the page cache. */
    device->optimal_block_size       = max(PAGE_SIZE, device->physical_block_size);
    device->blocks_per_optimal_block = device->optimal_block_size / device->block_size;

    device_publish(device->node);

    /* Scan for partitions. */
    partition_probe(device);
}

static status_t disk_init(void) {
    return device_class_init(&disk_device_class, DISK_DEVICE_CLASS_NAME);
}

static status_t disk_unload(void) {
    return device_class_destroy(&disk_device_class);
}

MODULE_NAME(DISK_MODULE_NAME);
MODULE_DESC("Disk device class manager");
MODULE_FUNCS(disk_init, disk_unload);
