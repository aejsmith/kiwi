/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory allocation functions.
 *
 * Simple set of malloc()/free() style functions implemented on top of the
 * slab allocator. The use of specialized slab caches is preferred over these
 * functions, however these are still useful for allocating temporary storage
 * when copying from userspace, or when allocating string buffers, etc.
 *
 * Cache sizes go up in powers of two, starting from 32 with a limitation of
 * 64K. For 64-bit systems, the boundary tag structure is 16 bytes, so having
 * caches smaller than 32 bytes is pointless. Allocations use the smallest
 * cache that can fit both the allocation and its information structure. If an
 * allocation larger than 64K is requested, then the allocation will use the
 * kernel memory allocator directly.
 *
 * Allocations are tracked using an alloc_tag_t structure, which is placed
 * before the allocation in memory. It tracks the size of the allocation and
 * the cache it came from. If the allocation came directly  from the kernel
 * memory allocator, then the cache pointer will be NULL.
 */

#include <device/device.h>

#include <lib/string.h>
#include <lib/utility.h>
#include <lib/fnv.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <assert.h>
#include <kernel.h>

/** Information structure prepended to allocations. */
typedef struct alloc_tag {
    uint32_t size;                  /**< Size of the allocation. */
    uint32_t magic;                 /**< Magic value. */
} alloc_tag_t;

/** Cache settings. */
enum {
    KMALLOC_CACHE_MIN = 5,          /**< Minimum cache size (2^5  == 32). */
    KMALLOC_CACHE_MAX = 16,         /**< Maximum cache size (2^16 == 64K). */

    KMALLOC_NUM_CACHES = KMALLOC_CACHE_MAX - KMALLOC_CACHE_MIN + 1
};

/** Slab caches for kmalloc(). */
static slab_cache_t *kmalloc_caches[KMALLOC_NUM_CACHES];

/**
 * Allocation magic values, set based on their cache. This gives a bit more
 * sanity checking of allocation compared to using a fixed value for everything.
 */
static uint32_t kmalloc_magic[KMALLOC_NUM_CACHES];
static const uint32_t KMALLOC_LARGE_MAGIC = 0xbeefbeef;

static size_t get_alloc_total(size_t size) {
    return size + sizeof(alloc_tag_t);
}

static size_t get_cache_idx(size_t total) {
    if (total > (1 << KMALLOC_CACHE_MAX)) {
        return KMALLOC_NUM_CACHES;
    } else {
        /* If exactly a power-of-two, then highbit(total) will work, else we
         * want the next size up. */
        size_t bit = highbit(total);
        size_t idx = (is_pow2(total)) ? bit - 1 : bit;
        return max(idx, KMALLOC_CACHE_MIN) - KMALLOC_CACHE_MIN;
    }
}

static alloc_tag_t *get_alloc_tag(void *addr) {
    return (alloc_tag_t *)((uint8_t *)addr - sizeof(alloc_tag_t));
}

static void validate_alloc(alloc_tag_t *tag) {
    bool valid = tag->size > 0;

    if (valid) {
        size_t total   = get_alloc_total(tag->size);
        size_t idx     = get_cache_idx(total);
        uint32_t magic = (idx < KMALLOC_NUM_CACHES) ? kmalloc_magic[idx] : KMALLOC_LARGE_MAGIC;

        valid = tag->magic == magic;
    }

    if (!valid)
        fatal("Invalid kmalloc allocation %p detected", &tag[1]);
}

/** Allocates a block of memory.
 * @param size          Size of block.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to block on success, NULL on failure. */
void *kmalloc(size_t size, uint32_t mmflag) {
    if (size == 0)
        return NULL;

    size_t total = get_alloc_total(size);
    size_t idx   = get_cache_idx(total);

    assert(total <= UINT32_MAX);

    alloc_tag_t *addr;
    if (idx < KMALLOC_NUM_CACHES) {
        addr = slab_cache_alloc(kmalloc_caches[idx], mmflag);
        if (unlikely(!addr))
            return NULL;

        addr->magic = kmalloc_magic[idx];
    } else {
        /* Fall back on kmem. */
        size_t aligned = round_up(total, PAGE_SIZE);
        addr = kmem_alloc(aligned, mmflag & MM_FLAG_MASK);
        if (unlikely(!addr))
            return NULL;

        addr->magic = KMALLOC_LARGE_MAGIC;
    }

    addr->size = size;

    /* Zero the allocation if requested. */
    if (mmflag & MM_ZERO)
        memset(&addr[1], 0, size);

    return &addr[1];
}

/** Allocates an array of zeroed memory.
 * @param nmemb         Number of array elements.
 * @param size          Size of each element.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to block on success, NULL on failure. */
void *kcalloc(size_t nmemb, size_t size, uint32_t mmflag) {
    return kmalloc(nmemb * size, mmflag | MM_ZERO);
}

/**
 * Resizes a memory block previously allocated with kmalloc(), kcalloc() or
 * krealloc(). If passed a NULL pointer, call is equivalent to
 * kmalloc(size, mmflag). If MM_ZERO is specified, and the block size is being
 * increased, then the space difference will be zeroed.
 *
 * @param addr          Address to resize.
 * @param size          New size.
 * @param mmflag        Allocation behaviour flags.
 *
 * @return              Pointer to block on success, NULL on failure.
 */
void *krealloc(void *addr, size_t size, uint32_t mmflag) {
    if (!addr) {
        return kmalloc(size, mmflag);
    } else if (size == 0) {
        kfree(addr);
        return NULL;
    }

    alloc_tag_t *tag = get_alloc_tag(addr);
    validate_alloc(tag);

    if (tag->size == size)
        return addr;

    /* Make a new allocation. */
    void *ret = kmalloc(size, mmflag & ~MM_ZERO);
    if (!ret)
        return ret;

    /* Copy the block data using the smallest of the two sizes. */
    memcpy(ret, addr, min(tag->size, size));

    /* Zero any new space if requested. */
    if (mmflag & MM_ZERO && size > tag->size)
        memset((char *)addr + tag->size, 0, size - tag->size);

    /* Free the old allocation. */
    kfree(addr);
    return ret;
}

/**
 * Frees a block of memory previously allocated with kmalloc(), kcalloc() or
 * krealloc().
 *
 * @param addr          Address to free. If NULL, nothing is done.
 */
void kfree(void *addr) {
    if (!addr)
        return;

    alloc_tag_t *tag = get_alloc_tag(addr);
    validate_alloc(tag);

    size_t total = get_alloc_total(tag->size);
    size_t idx   = get_cache_idx(total);

    /* Invalidate so we can detect double frees later. */
    tag->size  = 0;
    tag->magic = 0;

    if (idx < KMALLOC_NUM_CACHES) {
        slab_cache_free(kmalloc_caches[idx], tag);
    } else {
        size_t aligned = round_up(total, PAGE_SIZE);
        kmem_free(tag, aligned);
    }
}

/**
 * Allocate a block of memory as a device-managed resource. The memory will be
 * freed when the device is destroyed. The memory allocated with this function
 * *cannot* be used with krealloc() or kfree() - the only way it can be freed
 * is with the device when destroyed.
 *
 * @param device        Device to register to.
 * @param size          Size of block.
 * @param mmflag        Allocation behaviour flags.
 *
 * @return              Pointer to block on success, NULL on failure.
 */
void *device_kmalloc(device_t *device, size_t size, uint32_t mmflag) {
    /* We just allocate this directly with the tracking data. */
    void *mem = device_resource_alloc(size, NULL, mmflag);

    if (mem)
        device_resource_register(device, mem);

    return mem;
}

typedef struct device_kalloc_resource {
    void *addr;
} device_kalloc_resource_t;

static void device_kalloc_resource_release(device_t *device, void *data) {
    device_kalloc_resource_t *resource = data;

    kfree(resource->addr);
}

/**
 * Turns an existing memory allocation into a device-managed resource. The
 * memory will be freed when the device is destroyed. Once this is called, the
 * allocation must not be passed to krealloc() or kfree().
 *
 * The use case for this is where device data needs to be allocated before the
 * device itself is created (e.g. the device private data structure). Prefer
 * device_kmalloc() where possible, since it is more space efficient.
 *
 * @param device        Device to register to.
 * @param addr          Address of allocation.
 */
void device_add_kalloc(device_t *device, void *addr) {
    device_kalloc_resource_t *resource = device_resource_alloc(
        sizeof(device_kalloc_resource_t), device_kalloc_resource_release, MM_KERNEL);

    resource->addr = addr;

    device_resource_register(device, resource);
}

/** Initialize the allocator caches. */
__init_text void malloc_init(void) {
    for (size_t i = 0; i < array_size(kmalloc_caches); i++) {
        size_t size = (1 << (i + KMALLOC_CACHE_MIN));

        char name[SLAB_NAME_MAX];
        snprintf(name, SLAB_NAME_MAX, "kmalloc_%zu", size);
        name[SLAB_NAME_MAX - 1] = 0;

        kmalloc_caches[i] = slab_cache_create(name, size, 0, NULL, NULL, NULL, 0, MM_BOOT);
        kmalloc_magic[i]  = fnv32_hash_string(name);
    }
}
