/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Thread-local storage management.
 *
 * Reference:
 *  - ELF Handling For Thread-Local Storage
 *    http://people.redhat.com/drepper/tls.pdf
 *
 * TODO:
 *  - I don't think the offset calculation and alignment handling is quite
 *    right.
 *  - When cloning a process with kern_process_clone(), TLS data for all other
 *    threads should be freed.
 */

#include <kernel/private/thread.h>
#include <kernel/vm.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "libkernel.h"

/** Statically allocated DTV size. */
static ptr_t static_dtv_size;

/**
 * Get a TLS address.
 *
 * This is not called directly by code, it is called from an architecture-
 * specific wrapper (__tls_get_addr on most architectures).
 *
 * @param module        Module ID.
 * @param offset        Offset of object.
 *
 * @return              Address of current thread's copy of the variable.
 */
void *tls_get_addr(size_t module, size_t offset) {
    tls_tcb_t *tcb = arch_tls_tcb();
    size_t size;

    /* Check if the DTV contains this module. */
    size = (size_t)tcb->dtv[0];
    if (module >= size) {
        printf("DTV resizing is not yet implemented\n");
        return NULL;
    } else if (!tcb->dtv[module]) {
        printf("Dynamic TLS allocation not yet implemented\n");
        return NULL;
    }

    return (void *)(tcb->dtv[module] + offset);
}

#ifdef TLS_VARIANT_2

/** Work out the size to allocate for the initial TLS block.
 * @return              Size to allocate. */
static size_t initial_block_size(void) {
    rtld_image_t *image;
    size_t i, size = 0;

    /* tlsoffset(1) = round(tlssize(1), align(1))
     * tlsoffset(m+1) = round(tlsoffset(m) + tlssize(m+1), align(m+1)) */
    for (i = 1; i < static_dtv_size; i++) {
        image = rtld_image_lookup(i);
        if (image && image->tls_memsz)
            size = core_round_up(size + image->tls_memsz, image->tls_align);
    }

    /* Add on the TCB size. */
    size += sizeof(tls_tcb_t);
    return size;
}

/** Initialise the initial TLS block.
 * @param base          Base address of block.
 * @param dtv           Dynamic thread vector.
 * @return              TCB pointer. */
static tls_tcb_t *initial_block_init(ptr_t base, ptr_t *dtv) {
    rtld_image_t *image;
    size_t i;

    for (i = static_dtv_size - 1; i >= 1; i--) {
        image = rtld_image_lookup(i);
        if (!image || !image->tls_memsz)
            continue;

        /* Handle alignment requirements. */
        if (image->tls_align)
            base = core_round_up(base, image->tls_align);

        dprintf(
            "tls: loading image for module %s (%" PRId16 ") to %p (offset %p)\n",
            image->name, image->id, base, -image->tls_offset);

        dtv[i] = base;

        if (image->tls_filesz)
            memcpy((void *)base, image->tls_image, image->tls_filesz);

        if (image->tls_memsz - image->tls_filesz)
            memset((void *)(base + image->tls_filesz), 0, image->tls_memsz - image->tls_filesz);

        base += image->tls_memsz;
    }

    return (void *)base;
}

/** Get the offset of an image's TLS block from the thread pointer.
 * @param image         Image to get for.
 * @return              Offset from thread pointer, or 0 if the image is not
 *                      an initial image. */
ptrdiff_t tls_tp_offset(rtld_image_t *image) {
    ptrdiff_t offset = 0;
    image_id_t i;
    rtld_image_t *exist;

    if (static_dtv_size)
        return 0;

    /* tlsoffset(1) = round(tlssize(1), align(1))
     * tlsoffset(m+1) = round(tlsoffset(m) + tlssize(m+1), align(m+1)) */
    for (i = 1; i < image->id; i++) {
        exist = rtld_image_lookup(i);
        if (exist && exist->tls_memsz)
            offset = core_round_up(offset + exist->tls_memsz, exist->tls_align);
    }

    offset = core_round_up(offset + image->tls_memsz, image->tls_align);

    /* Want the negative of what we've worked out, the data is behind the thread
     * pointer. */
    offset = -offset;
    return offset;
}

#elif defined(TLS_VARIANT_1)

#error "TLS_VARIANT_1 is not implemented"

#endif

/** Allocate a TLS block for a new thread.
 * @param _tcb          Where to store TCB address.
 * @return              Status code describing result of the operation. */
status_t tls_alloc(tls_tcb_t **_tcb) {
    ptr_t *dtv;
    tls_tcb_t *tcb;
    size_t size;
    void *alloc;
    status_t ret;

    /*
     * All initial modules (the executable itself and the libraries loaded along
     * with it) must have their TLS blocks allocated statically. Any modules
     * loaded at runtime can have their TLS blocks dynamically created. If this
     * is the first call to this function, which will be just after loading all
     * initial modules, we record the size we need to statically allocate. Note
     * that the size will be (number of initial modules + 1) because the first
     * DTV entry is the "generation number". This is used to record the current
     * size of the DTV to allow it to be dynamically resized.
     */
    if (!static_dtv_size)
        static_dtv_size = next_image_id;

    /* Create the dynamic thread vector. */
    dtv = malloc(static_dtv_size * sizeof(ptr_t));
    if (!dtv)
        return STATUS_NO_MEMORY;

    /* Store the current size. */
    dtv[0] = static_dtv_size;

    /* Allocate the TLS block. */
    size = core_round_up(initial_block_size(), page_size);

    ret = kern_vm_map(
        &alloc, size, 0, VM_ADDRESS_ANY, VM_ACCESS_READ | VM_ACCESS_WRITE,
        VM_MAP_PRIVATE, INVALID_HANDLE, 0, NULL);
    if (ret != STATUS_SUCCESS) {
        free(dtv);
        return ret;
    }

    tcb = initial_block_init((ptr_t)alloc, dtv);
    arch_tls_tcb_init(tcb);
    tcb->dtv = dtv;
    tcb->base = alloc;

    *_tcb = tcb;
    return STATUS_SUCCESS;
}

/** Destroy a TLS block.
 * @param tcb           TCB to destroy. */
void tls_destroy(tls_tcb_t *tcb) {
    /* TODO: Will need to free dynamically allocated blocks here. */
    free(tcb->dtv);
    kern_vm_unmap(tcb->base, core_round_up(initial_block_size(), page_size));
}
