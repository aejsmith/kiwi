/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network packet management.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <net/packet.h>

#include <assert.h>

static slab_cache_t *net_buffer_kmalloc_cache;
static slab_cache_t *net_buffer_external_cache;
static slab_cache_t *net_buffer_ref_cache;
static slab_cache_t *net_packet_cache;

/**
 * Allocates a new network buffer with a kmalloc()'d data buffer.
 *
 * The caller must ensure that the buffer data is fully written to prevent
 * leakage of kernel data - use MM_ZERO if the buffer will not be fully written
 * to.
 *
 * The buffer is not owned by a packet, so should be destroyed with
 * net_buffer_destroy() if it is no longer needed before it is attached to a
 * packet.
 *
 *
 * @param size          Size of the buffer to allocate.
 * @param mmflag        Memory allocation flags.
 * @param _data         Where to return pointer to data buffer.
 *
 * @return              Allocated network buffer, or NULL on failure (only
 *                      if mmflag allows failure).
 */
__export net_buffer_t *net_buffer_kmalloc(uint32_t size, uint32_t mmflag, void **_data) {
    assert(size > 0);

    void *data = kmalloc(size, mmflag);
    if (!data)
        return NULL;

    if (_data)
        *_data = data;

    return net_buffer_from_kmalloc(data, size);
}

/**
 * Allocates a new network buffer from a slab cache. The slab cache should
 * allocate (sizeof(net_buffer_slab_t) + data buffer size) bytes.
 *
 * This is intended for use where that are frequent allocations of a fixed
 * buffer size, namely protocol headers. Dedicated slab caches can be used for
 * faster allocation of these.
 *
 * The caller must ensure that the buffer data is fully written to prevent
 * leakage of kernel data.
 *
 * The buffer is not owned by a packet, so should be destroyed with
 * net_buffer_destroy() if it is no longer needed before it is attached to a
 * packet.
 *
 * @param cache         Slab cache to allocate from.
 * @param size          Size of the data buffer. This needs to be specified
 *                      explicitly as slab will internally align up the object
 *                      size, and therefore pulling the size from the cache may
 *                      not yield the correct size.
 * @param mmflag        Memory allocation flags.
 * @param _data         Where to return pointer to data buffer.
 *
 * @return              Allocated network buffer, or NULL on failure (only
 *                      if mmflag allows failure).
 */
__export net_buffer_t *net_buffer_slab_alloc(slab_cache_t *cache, uint32_t size, uint32_t mmflag, void **_data) {
    assert(size > 0);
    assert(cache->obj_size >= sizeof(net_buffer_slab_t) + size);

    net_buffer_slab_t *buffer = slab_cache_alloc(cache, mmflag);
    if (!buffer)
        return NULL;

    net_buffer_init(&buffer->buffer);

    buffer->buffer.type = NET_BUFFER_TYPE_SLAB;
    buffer->buffer.size = size;
    buffer->cache       = cache;

    if (_data)
        *_data = buffer->data;

    return &buffer->buffer;
}

/**
 * Creates a new network buffer taking ownership of a pre-kmalloc()'d data
 * buffer (the data buffer will be kfree()'d when the buffer is destroyed).
 *
 * The buffer is not owned by a packet, so should be destroyed with
 * net_buffer_destroy() if it is no longer needed before it is attached to a
 * packet.
 *
 * @param data          Data buffer.
 * @param size          Size of the buffer to allocate.
 *
 * @return              Allocated network buffer.
 */
__export net_buffer_t *net_buffer_from_kmalloc(void *data, uint32_t size) {
    net_buffer_kmalloc_t *buffer = slab_cache_alloc(net_buffer_kmalloc_cache, MM_KERNEL);

    net_buffer_init(&buffer->buffer);

    buffer->buffer.type = NET_BUFFER_TYPE_KMALLOC;
    buffer->buffer.size = size;
    buffer->data        = data;

    return &buffer->buffer;
}

/**
 * Creates a new network buffer referring to a subset of an existing packet.
 * This is used to create a new packet which can have new data buffers added to
 * it without affecting the underlying packet. The source packet's reference
 * count is incremented, which means its offset/size must not be modified while
 * the subset buffer exists.
 *
 * The buffer is not owned by a packet, so should be destroyed with
 * net_buffer_destroy() if it is no longer needed before it is attached to a
 * packet.
 *
 * @param packet        Source packet to reference.
 * @param offset        Offset within the packet (must be within the source).
 * @param size          Size of the subset (offset + size must be within the
 *                      source).
 *
 * @return              Allocated network buffer.
 */
__export net_buffer_t *net_buffer_from_subset(net_packet_t *packet, uint32_t offset, uint32_t size) {
    assert(offset < packet->size);
    assert(offset + size <= packet->size);

    net_packet_retain(packet);

    net_buffer_ref_t *buffer = slab_cache_alloc(net_buffer_ref_cache, MM_KERNEL);

    net_buffer_init(&buffer->buffer);

    buffer->buffer.type   = NET_BUFFER_TYPE_REF;
    buffer->buffer.size   = size;
    buffer->packet        = packet;
    buffer->packet_offset = offset;

    return &buffer->buffer;
}

static void free_external_buffer(net_buffer_external_t *buffer) {
    slab_cache_free(net_buffer_external_cache, buffer);
}

/**
 * Creates a new network buffer referring to an external memory buffer. It is
 * the responsibility of the caller to ensure that this buffer is valid for the
 * lifetime of the packet.
 *
 * The buffer is not owned by a packet, so should be destroyed with
 * net_buffer_destroy() if it is no longer needed before it is attached to a
 * packet.
 *
 * @param data          Data for the buffer.
 * @param size          Size of the data.
 *
 * @return              Allocated network buffer.
 */
net_buffer_t *net_buffer_from_external(void *data, uint32_t size) {
    /* Reuse the same mechanism that allows the buffers structures to be
     * externally allocated, but allocate the buffer header from a slab cache. */
    net_buffer_external_t *buffer = slab_cache_alloc(net_buffer_external_cache, MM_KERNEL);

    net_buffer_init(&buffer->buffer);

    buffer->buffer.type = NET_BUFFER_TYPE_EXTERNAL;
    buffer->buffer.size = size;
    buffer->data        = data;
    buffer->free        = free_external_buffer;

    return &buffer->buffer;
}

/**
 * Destroys a network buffer. This should only be used either before the buffer
 * has been attached to a packet, or internally by the packet implementation:
 * buffers are owned by a packet once attached to it.
 *
 * @param buffer        Buffer to destroy.
 */
__export void net_buffer_destroy(net_buffer_t *buffer) {
    switch (buffer->type) {
        case NET_BUFFER_TYPE_KMALLOC: {
            net_buffer_kmalloc_t *derived = (net_buffer_kmalloc_t *)buffer;
            kfree(derived->data);
            slab_cache_free(net_buffer_kmalloc_cache, derived);
            break;
        }
        case NET_BUFFER_TYPE_SLAB: {
            net_buffer_slab_t *derived = (net_buffer_slab_t *)buffer;
            slab_cache_free(derived->cache, derived);
            break;
        }
        case NET_BUFFER_TYPE_EXTERNAL: {
            net_buffer_external_t *derived = (net_buffer_external_t *)buffer;
            derived->free(derived);
            break;
        }
        case NET_BUFFER_TYPE_REF: {
            net_buffer_ref_t *derived = (net_buffer_ref_t *)buffer;
            net_packet_release(derived->packet);
            slab_cache_free(net_buffer_ref_cache, derived);
            break;
        }
        default: {
            unreachable();
            break;
        }
    }
}

/** Retrieves a data pointer from a buffer. */
uint8_t *net_buffer_data(net_buffer_t *buffer, uint32_t offset, uint32_t size) {
    offset += buffer->offset;

    assert(offset + size <= buffer->size);

    switch (buffer->type) {
        case NET_BUFFER_TYPE_KMALLOC: {
            net_buffer_kmalloc_t *derived = (net_buffer_kmalloc_t *)buffer;
            return (uint8_t *)derived->data + offset;
        }
        case NET_BUFFER_TYPE_SLAB: {
            net_buffer_slab_t *derived = (net_buffer_slab_t *)buffer;
            return &derived->data[offset];
        }
        case NET_BUFFER_TYPE_EXTERNAL: {
            net_buffer_external_t *derived = (net_buffer_external_t *)buffer;
            return (uint8_t *)derived->data + offset;
        }
        case NET_BUFFER_TYPE_REF: {
            net_buffer_ref_t *derived = (net_buffer_ref_t *)buffer;
            return net_packet_data(derived->packet, derived->packet_offset + offset, size);
        }
        default: {
            unreachable();
            break;
        }
    }
}

/**
 * Increases the reference count of a packet. While the reference count is
 * above 1, the packet cannot be modified.
 *
 * @param packet        Packet to retain.
 */
__export void net_packet_retain(net_packet_t *packet) {
    packet->refcount++;
}

/**
 * Decreases the reference count of a network packet, and destroys it if it
 * reaches 0.
 *
 * @param packet        Packet to release.
 */
__export void net_packet_release(net_packet_t *packet) {
    assert(packet->refcount > 0);

    if (--packet->refcount == 0) {
        net_buffer_t *next = packet->head;

        while (next) {
            net_buffer_t *buffer = next;
            next = buffer->next;
            net_buffer_destroy(buffer);
        }

        slab_cache_free(net_packet_cache, packet);
    }
}

/**
 * Creates a new network packet containing the given buffer. Ownership of the
 * buffer will be taken over by the packet. The packet will have one reference
 * on it (and is therefore mutable).
 *
 * @param buffer        Initial packet data buffer.
 *
 * @return              Pointer to created packet.
 */
__export net_packet_t *net_packet_create(net_buffer_t *buffer) {
    assert(buffer);
    assert(buffer->size > 0);
    assert(buffer->offset < buffer->size);

    net_packet_t *packet = slab_cache_alloc(net_packet_cache, MM_KERNEL);

    packet->head     = buffer;
    packet->refcount = 1;
    packet->size     = buffer->size - buffer->offset;
    packet->type     = NET_PACKET_TYPE_UNKNOWN;

    return packet;
}

/**
 * Offsets the start of a packet further into the packet data, e.g. to remove
 * protocol headers.
 *
 * This cannot be reversed. If the offset is advanced beyond a buffer boundary
 * it will be freed.
 *
 * @param packet        Packet to offset. Must have a reference count of 1.
 * @param offset        Amount to offset. This must be less than the packet
 *                      total size, the caller is responsible for ensuring this
 *                      and rejecting packets where this is not the case.
 */
__export void net_packet_offset(net_packet_t *packet, uint32_t offset) {
    assert(packet->refcount == 1);
    assert(offset > 0);
    assert(offset < packet->size);

    while (offset > 0) {
        net_buffer_t *buffer = packet->head;

        uint32_t remaining  = buffer->size - buffer->offset;
        uint32_t buf_offset = min(remaining, offset);

        buffer->offset += buf_offset;

        if (buffer->offset == buffer->size) {
            assert(buffer->next);

            packet->head = buffer->next;
            net_buffer_destroy(buffer);
        }

        offset       -= buf_offset;
        packet->size -= buf_offset;
    }
}

/**
 * Resizes the packet to a subset of the data, i.e. offsets it and adjusts the
 * size. The caller must verify that the subset is within the packet before
 * calling this function.
 *
 * This cannot be reversed. Buffers outside of the subset will be freed.
 *
 * @param packet        Packet to subset. Must have a reference count of 1.
 * @param offset        Start offset of the subset.
 * @param size          Size of the subset.
 */
void net_packet_subset(net_packet_t *packet, uint32_t offset, uint32_t size) {
    assert(packet->refcount == 1);
    assert(offset >= 0);
    assert(size > 0);
    assert(offset < packet->size);
    assert(offset + size <= packet->size);

    /* Perform the offset. */
    net_packet_offset(packet, offset);

    /* Trim off any excess buffers at the end. */
    net_buffer_t *next = packet->head;
    uint32_t total = 0;
    while (total < size) {
        assert(next);
        net_buffer_t *buffer = next;
        next = buffer->next;
        total += buffer->size - buffer->offset;
        if (total >= size)
            buffer->next = NULL;
    }

    while (next) {
        net_buffer_t *buffer = next;
        next = buffer->next;
        net_buffer_destroy(buffer);
    }

    packet->size = size;
}

/**
 * Prepends a data buffer to a packet, e.g. to add a protocol header. The
 * buffer must not be in use by any other packet, ownership of it will be taken
 * by the packet.
 *
 * @param packet        Packet to prepend to. Must have a reference count of 1.
 * @param buffer        Buffer to prepend.
 */
__export void net_packet_prepend(net_packet_t *packet, net_buffer_t *buffer) {
    assert(packet->refcount == 1);
    assert(buffer);
    assert(buffer->size > 0);
    assert(buffer->offset < buffer->size);

    packet->size += buffer->size - buffer->offset;

    buffer->next = packet->head;
    packet->head = buffer;
}

/**
 * Appends a data buffer to a packet, e.g. to add a packet data payload. The
 * buffer must not be in use by any other packet, ownership of it will be taken
 * by the packet.
 *
 * @param packet        Packet to append to. Must have a reference count of 1.
 * @param buffer        Buffer to append.
 */
__export void net_packet_append(net_packet_t *packet, net_buffer_t *buffer) {
    assert(packet->refcount == 1);
    assert(buffer);
    assert(buffer->size > 0);
    assert(buffer->offset < buffer->size);

    packet->size += buffer->size - buffer->offset;

    buffer->next = NULL;

    if (packet->head == NULL) {
        packet->head = buffer;
    } else {
        net_buffer_t *curr = packet->head;
        while (curr->next)
            curr = curr->next;
        curr->next = buffer;
    }
}

/**
 * Retrieves a contiguous block of data from a packet.
 *
 * This can only be done if the requested range is within a single buffer. It
 * can generally be assumed that this is the case for protocol headers: on
 * transmit, these are added as one buffer each, while on receive, it is
 * expected that network device drivers pass in the whole received packet as a
 * single buffer.
 *
 * If the requested range is not within a single buffer, or outside of the
 * range of the packet, then this will return NULL. On received packets this
 * must be gracefully handled as a malformed packet.
 *
 * @param packet        Packet to retrived data from.
 * @param offset        Offset into the packet.
 * @param size          Size of the range.
 *
 * @return              Pointer to data, or NULL if range is either outside of
 *                      the packet or not within a single buffer.
 */
__export void *net_packet_data(net_packet_t *packet, uint32_t offset, uint32_t size) {
    assert(size > 0);

    if (offset >= packet->size || offset + size > packet->size)
        return NULL;

    net_buffer_t *buffer = packet->head;

    while (true) {
        uint32_t remaining = buffer->size - buffer->offset;

        if (offset < remaining) {
            if (offset + size > remaining)
                return NULL;

            return net_buffer_data(buffer, offset, size);
        }

        assert(buffer->next);

        offset -= remaining;
        buffer  = buffer->next;
    }
}

/**
 * Copies data out of network packet to a contiguous buffer. The specified
 * range must be within the packet.
 *
 * @param packet        Packet to copy from.
 * @param dest          Destination buffer.
 * @param offset        Offset within the packet to copy from.
 * @param size          Size of the range to copy.
 */
__export void net_packet_copy_from(net_packet_t *packet, void *dest, uint32_t offset, uint32_t size) {
    assert(size > 0);
    assert(offset < packet->size);
    assert(offset + size <= packet->size);

    uint8_t *d = dest;

    net_buffer_t *buffer = packet->head;

    while (size > 0) {
        assert(buffer);

        uint32_t remaining = buffer->size - buffer->offset;

        if (offset < remaining) {
            uint32_t buf_size = min(size, remaining - offset);

            if (buffer->type == NET_BUFFER_TYPE_REF) {
                /* References may be non-contiguous in the underlying packet so
                 * we must recurse to handle that. */
                net_buffer_ref_t *derived = (net_buffer_ref_t *)buffer;
                net_packet_copy_from(
                    derived->packet, d,
                    derived->packet_offset + buffer->offset + offset,
                    buf_size);
            } else {
                /* Otherwise we have a contiguous range to copy. */
                uint8_t *buf_data = net_buffer_data(buffer, offset, buf_size);
                assert(buf_data);
                memcpy(d, buf_data, buf_size);
            }

            size   -= buf_size;
            d      += buf_size;
            offset  = 0;
        } else {
            offset -= remaining;
        }

        buffer = buffer->next;
    }
}

/** Initialize network packet slab caches. */
void net_packet_cache_init(void) {
    net_buffer_kmalloc_cache = object_cache_create(
        "net_buffer_kmalloc_cache", net_buffer_kmalloc_t, NULL, NULL, NULL, 0,
        MM_KERNEL);
    net_buffer_external_cache = object_cache_create(
        "net_buffer_external_cache", net_buffer_external_t, NULL, NULL, NULL, 0,
        MM_KERNEL);
    net_buffer_ref_cache = object_cache_create(
        "net_buffer_ref_cache", net_buffer_ref_t, NULL, NULL, NULL, 0,
        MM_KERNEL);
    net_packet_cache = object_cache_create(
        "net_packet_cache", net_packet_t, NULL, NULL, NULL, 0,
        MM_KERNEL);
}
