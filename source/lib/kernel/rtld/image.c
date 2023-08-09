/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               RTLD image management.
 *
 * TODO:
 *  - Report missing library/symbol names back to the creator of the process.
 *  - When the API is implemented, need to wrap calls in a lock.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/private/image.h>
#include <kernel/vm.h>

#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#define _GNU_SOURCE
#include <link.h>

#include "libkernel.h"

/** Expected path to libkernel. */
#define LIBKERNEL_PATH      "/system/lib/libkernel.so"

/** Array of directories to search for libraries in. */
static const char *library_search_dirs[] = {
    "/system/lib",
    NULL
};

/** Next image ID. */
image_id_t next_image_id = DYNAMIC_IMAGE_START;

/** List of loaded images. */
CORE_LIST_DEFINE(loaded_images);

/** Image structure representing the kernel library. */
rtld_image_t libkernel_image = {
    .id       = LIBKERNEL_IMAGE_ID,
    .name     = "libkernel.so",
    .path     = LIBKERNEL_PATH,
    .refcount = 0,
    .state    = RTLD_IMAGE_LOADED,
};

/** Pointer to the application image. */
rtld_image_t *application_image;

/** Look up an image by ID.
 * @param id            ID of image to look up.
 * @return              Pointer to image structure if found. */
rtld_image_t *rtld_image_lookup(image_id_t id) {
    core_list_foreach(&loaded_images, iter) {
        rtld_image_t *image = core_list_entry(iter, rtld_image_t, header);

        if (image->id == id)
            return image;
    }

    return NULL;
}

/** Handle an ELF_PT_LOAD program header.
 * @param image         Image being loaded.
 * @param phdr          Program header.
 * @param handle        Handle to the file.
 * @param i             Index of the program header.
 * @return              Status code describing result of the operation. */
static status_t do_load_phdr(rtld_image_t *image, elf_phdr_t *phdr, handle_t handle, size_t i) {
    status_t ret;

    elf_addr_t load_base = (elf_addr_t)image->load_base;

    /* Work out the access flags to use. */
    uint32_t access = 0;
    if (phdr->p_flags & ELF_PF_R)
        access |= VM_ACCESS_READ;
    if (phdr->p_flags & ELF_PF_W)
        access |= VM_ACCESS_WRITE;
    if (phdr->p_flags & ELF_PF_X)
        access |= VM_ACCESS_EXECUTE;
    if (!access) {
        dprintf("rtld: %s: program header %zu has no protection flags\n", image->path, i);
        return STATUS_MALFORMED_IMAGE;
    }

    /* Map the BSS if required. */
    if (phdr->p_memsz > phdr->p_filesz) {
        elf_addr_t bss_start = load_base + core_round_down(phdr->p_vaddr + phdr->p_filesz, page_size);
        elf_addr_t bss_end   = load_base + core_round_up(phdr->p_vaddr + phdr->p_memsz, page_size);
        size_t bss_size      = bss_end - bss_start;

        /* Must be writable to be able to clear later. */
        if (!(access & VM_ACCESS_WRITE)) {
            dprintf("rtld: %s: program header %zu should be writable\n", image->path, i);
            return STATUS_MALFORMED_IMAGE;
        }

        /* Create an anonymous region for it. */
        ret = kern_vm_map(
            (void **)&bss_start, bss_size, 0, VM_ADDRESS_EXACT, access,
            VM_MAP_PRIVATE, INVALID_HANDLE, 0, NULL);
        if (ret != STATUS_SUCCESS) {
            dprintf("rtld: %s: unable to create anonymous BSS region: %d\n", image->path, ret);
            return ret;
        }
    }

    /* Won't need to clear BSS if file size is 0, since we've just mapped an
     * anonymous zeroed region. */
    if (phdr->p_filesz == 0)
        return STATUS_SUCCESS;

    elf_addr_t start = load_base + core_round_down(phdr->p_vaddr, page_size);
    elf_addr_t end   = load_base + core_round_up(phdr->p_vaddr + phdr->p_filesz, page_size);
    size_t size      = end - start;
    offset_t offset  = core_round_down(phdr->p_offset, page_size);

    dprintf("rtld: %s: loading header %zu to [%p,%p)\n", image->path, i, start, start + size);

    /* Map the data in. Set the private flag if mapping as writeable. */
    ret = kern_vm_map(
        (void **)&start, size, 0, VM_ADDRESS_EXACT, access,
        (access & VM_ACCESS_WRITE) ? VM_MAP_PRIVATE : 0, handle, offset, NULL);
    if (ret != STATUS_SUCCESS) {
        dprintf("rtld: %s: unable to map file data into memory: %d\n", image->path, ret);
        return ret;
    }

    /* Clear out BSS. */
    if (phdr->p_filesz < phdr->p_memsz) {
        elf_addr_t clear_start = load_base + phdr->p_vaddr + phdr->p_filesz;
        size_t clear_size      = phdr->p_memsz - phdr->p_filesz;

        dprintf(
            "rtld: %s: clearing BSS for %zu at [%p,%p)\n",
            image->path, i, clear_start, clear_start + clear_size);

        memset((void *)clear_start, 0, clear_size);
    }

    return STATUS_SUCCESS;
}

/** Load an image.
 * @param path          Path to image file.
 * @param type          Required ELF type.
 * @param _entry        Where to store entry point for binary, if type is
 *                      ELF_ET_EXEC.
 * @param _image        Where to store pointer to image structure.
 * @return              Status code describing result of the operation. */
static status_t load_image(const char *path, int type, void **_entry, rtld_image_t **_image) {
    status_t ret;
    size_t bytes;

    rtld_image_t *image = NULL;

    /* Try to open the image. */
    handle_t handle;
    ret = kern_fs_open(path, FILE_ACCESS_READ | FILE_ACCESS_EXECUTE, 0, 0, &handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    file_info_t file;
    ret = kern_file_info(handle, &file);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* Search to see if this file is already loaded. */
    core_list_foreach(&loaded_images, iter) {
        rtld_image_t *exist = core_list_entry(iter, rtld_image_t, header);

        if (exist->node == file.id && exist->mount == file.mount) {
            if (exist->state == RTLD_IMAGE_LOADING) {
                dprintf("rtld: cyclic dependency on %s detected!\n", exist->name);
                ret = STATUS_MALFORMED_IMAGE;
                goto fail;
            }

            dprintf("rtld: increasing reference count on %s (%p)\n", exist->name, exist);

            exist->refcount++;

            if (_image)
                *_image = exist;

            kern_handle_close(handle);
            return STATUS_SUCCESS;
        }
    }

    /* Read in its header and ensure that it is valid. */
    elf_ehdr_t ehdr;
    ret = kern_file_read(handle, &ehdr, sizeof(ehdr), 0, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != sizeof(ehdr)) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (strncmp((const char *)ehdr.e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
        dprintf("rtld: %s: not a valid ELF file\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_ident[ELF_EI_CLASS] != ELF_CLASS) {
        dprintf("rtld: %s: incorrect ELF class\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_ident[ELF_EI_DATA] != ELF_ENDIAN) {
        dprintf("rtld: %s: incorrect endianness\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_machine != ELF_MACHINE) {
        dprintf("rtld: %s: not for this machine\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
        dprintf("rtld: %s: not correct ELF version\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_type != type) {
        dprintf("rtld: %s: incorrect ELF file type\n", path);
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (ehdr.e_phentsize != sizeof(elf_phdr_t)) {
        dprintf("rtld: %s: bad program header size\n", path);
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* Create a structure to track information about the image. */
    image = calloc(1, sizeof(*image));
    if (!image) {
        ret = STATUS_NO_MEMORY;
        goto fail;
    }

    /* This must be written immediately - it is used to set application_image
     * which is checked in symbol lookup. */
    if (_image)
        *_image = image;

    core_list_init(&image->header);

    image->id    = (ehdr.e_type == ELF_ET_EXEC) ? APPLICATION_IMAGE_ID : next_image_id++;
    image->node  = file.id;
    image->mount = file.mount;

    /* Don't particularly care if we can't duplicate the path string, its not
     * important (only for debugging purposes). */
    image->path = strdup(path);

    /* Read in the program headers. */
    size_t phdr_size  = ehdr.e_phnum * ehdr.e_phentsize;
    elf_phdr_t *phdrs = alloca(phdr_size);
    ret = kern_file_read(handle, phdrs, phdr_size, ehdr.e_phoff, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != phdr_size) {
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* If loading a library, find out exactly how much space we need for all the
     * the LOAD headers, and allocate a chunk of memory for them. For
     * executables, just put the load base as NULL. */
    if (ehdr.e_type == ELF_ET_DYN) {
        image->load_size = 0;
        for (size_t i = 0; i < ehdr.e_phnum; i++) {
            if (phdrs[i].p_type != ELF_PT_LOAD)
                continue;

            if ((phdrs[i].p_vaddr + phdrs[i].p_memsz) > image->load_size)
                image->load_size = core_round_up(phdrs[i].p_vaddr + phdrs[i].p_memsz, page_size);
        }

        /* Allocate a chunk of address space for it. */
        ret = kern_vm_map(
            &image->load_base, image->load_size, 0, VM_ADDRESS_ANY,
            VM_ACCESS_READ, VM_MAP_PRIVATE, INVALID_HANDLE, 0, NULL);
        if (ret != STATUS_SUCCESS) {
            dprintf("rtld: %s: unable to allocate address space: %d\n", path, ret);
            goto fail;
        }
    } else {
        image->load_base = NULL;
        image->load_size = 0;
    }

    char *interp = NULL;

    /* Load all of the LOAD headers, and save the address of the dynamic section
     * if we find it. */
    for (size_t i = 0; i < ehdr.e_phnum; i++) {
        switch (phdrs[i].p_type) {
            case ELF_PT_LOAD:
                ret = do_load_phdr(image, &phdrs[i], handle, i);
                if (ret != STATUS_SUCCESS)
                    goto fail;

                /* Assume the first LOAD header in the image covers the EHDR and
                 * the PHDRs. */
                if (!image->ehdr && !image->phdrs) {
                    image->ehdr      = image->load_base + core_round_down(phdrs[i].p_vaddr, page_size);
                    image->phdrs     = (void *)image->ehdr + ehdr.e_phoff;
                    image->num_phdrs = ehdr.e_phnum;
                }

                break;

            case ELF_PT_INTERP:
                if (ehdr.e_type == ELF_ET_EXEC) {
                    interp = alloca(phdrs[i].p_filesz + 1);

                    ret = kern_file_read(handle, interp, phdrs[i].p_filesz, phdrs[i].p_offset, NULL);
                    if (ret != STATUS_SUCCESS)
                        goto fail;

                    interp[phdrs[i].p_filesz] = 0;
                } else if (ehdr.e_type == ELF_ET_DYN) {
                    dprintf("rtld: %s: library requires an interpreter!\n", path);
                    ret = STATUS_MALFORMED_IMAGE;
                    goto fail;
                }

                break;

            case ELF_PT_DYNAMIC:
                image->dyntab = image->load_base + phdrs[i].p_vaddr;
                break;

            case ELF_PT_TLS:
                if (!phdrs[i].p_memsz) {
                    break;
                } else if (image->tls_memsz) {
                    /* TODO: Is this right? */
                    dprintf("rtld: %s: multiple TLS segments not allowed\n", path);
                    ret = STATUS_MALFORMED_IMAGE;
                    goto fail;
                }

                /* Record information about the initial TLS image. */
                image->tls_image  = image->load_base + phdrs[i].p_vaddr;
                image->tls_filesz = phdrs[i].p_filesz;
                image->tls_memsz  = phdrs[i].p_memsz;
                image->tls_align  = phdrs[i].p_align;
                image->tls_offset = tls_tp_offset(image);

                dprintf(
                    "rtld: %s: got TLS segment at %p (filesz: %zu, memsz: %zu, align: %zu)\n",
                    path, image->tls_image, image->tls_filesz, image->tls_memsz,
                    image->tls_align);

                break;

            case ELF_PT_NOTE:
            case ELF_PT_PHDR:
                break;

            case ELF_PT_GNU_EH_FRAME:
            case ELF_PT_GNU_STACK:
                // FIXME: Handle stack.
                break;

            default:
                dprintf("rtld: %s: program header %zu has unhandled type %u\n", path, phdrs[i].p_type);
                ret = STATUS_MALFORMED_IMAGE;
                goto fail;
        }
    }

    /* If loading an executable, check that it has libkernel as its interpreter.
     * This is to prevent someone from attempting to run a non-Kiwi application. */
    if (ehdr.e_type == ELF_ET_EXEC) {
        if (!interp || strcmp(interp, LIBKERNEL_PATH) != 0) {
            printf("rtld: %s: not a Kiwi application\n", path);
            ret = STATUS_MALFORMED_IMAGE;
            goto fail;
        }
    }

    /* Check that there was a DYNAMIC header. */
    if (!image->dyntab) {
        dprintf("rtld: %s: could not find DYNAMIC section\n", path);
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* Fill in our dynamic table and do address fixups. We copy some of the
     * table entries we need into a table indexed by tag for easy access. */
    for (size_t i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
        if (image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED)
            continue;

        /* Do address fixups. */
        switch (image->dyntab[i].d_tag) {
            case ELF_DT_HASH:
            case ELF_DT_PLTGOT:
            case ELF_DT_STRTAB:
            case ELF_DT_SYMTAB:
            case ELF_DT_JMPREL:
            case ELF_DT_REL_TYPE:
                image->dyntab[i].d_un.d_ptr += (elf_addr_t)image->load_base;
                break;
        }

        image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
    }

    /* Set name and loading state, and fill out hash information. FIXME: Just
     * use basename of path for application, and for library if SONAME not set. */
    image->name = (type == ELF_ET_DYN)
        ? (const char *)(image->dynamic[ELF_DT_SONAME] + image->dynamic[ELF_DT_STRTAB])
        : "<application>";
    image->state = RTLD_IMAGE_LOADING;
    rtld_symbol_init(image);

    /* Check if the image is already loaded. */
    if (type == ELF_ET_DYN) {
        core_list_foreach(&loaded_images, iter) {
            rtld_image_t *exist = core_list_entry(iter, rtld_image_t, header);

            if (strcmp(exist->name, image->name) == 0) {
                printf("rtld: %s: image with same name already loaded\n", path);
                ret = STATUS_ALREADY_EXISTS;
                goto fail;
            }
        }
    }

    core_list_append(&loaded_images, &image->header);

    /* Load libraries that we depend on. */
    for (size_t i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
        if (image->dyntab[i].d_tag != ELF_DT_NEEDED)
            continue;

        const char *dep = (const char *)(image->dyntab[i].d_un.d_ptr + image->dynamic[ELF_DT_STRTAB]);

        dprintf("rtld: %s: dependency on %s\n", path, dep);

        ret = rtld_image_load(dep, NULL);
        if (ret != STATUS_SUCCESS) {
            if (ret == STATUS_NOT_FOUND) {
                printf(
                    "rtld: could not find required library %s (required by %s)\n",
                    dep, image->name);

                ret = STATUS_MISSING_LIBRARY;
            }

            goto fail;
        }
    }

    /* We can now perform relocations. */
    ret = arch_rtld_image_relocate(image);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* We are loaded. */
    image->refcount = 1;
    image->state    = RTLD_IMAGE_LOADED;

    /* Register the image with the kernel. FIXME: See above about basename.
     * TODO: Load in full symbol table if possible as the dynsym table only
     * contains exported symbols (perhaps only for debug builds). */
    image_info_t info;
    info.name        = (type == ELF_ET_DYN) ? image->name : image->path;
    info.path        = image->path;
    info.load_base   = image->load_base;
    info.load_size   = image->load_size;
    info.symtab      = (void *)image->dynamic[ELF_DT_SYMTAB];
    info.sym_entsize = image->dynamic[ELF_DT_SYMENT];
    info.sym_size    = image->h_nchain * info.sym_entsize;
    info.strtab      = (void *)image->dynamic[ELF_DT_STRTAB];

    ret = kern_image_register(image->id, &info);
    if (ret != STATUS_SUCCESS) {
        printf("rtld: failed to register image with kernel: %d\n", ret);
        goto fail;
    }

    if (_entry)
        *_entry = (void *)ehdr.e_entry;

    kern_handle_close(handle);
    return STATUS_SUCCESS;

fail:
    if (image) {
        if (image->load_base)
            kern_vm_unmap(image->load_base, image->load_size);

        core_list_remove(&image->header);
        free(image);
    }

    kern_handle_close(handle);
    return ret;
}

static bool path_exists(const char *path) {
    dprintf("  trying %s... ", path);

    /* Attempt to open it to see if it is there. */
    handle_t handle;
    status_t ret = kern_fs_open(path, FILE_ACCESS_READ, 0, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        dprintf("returned %d\n", ret);
        return false;
    }

    dprintf("success!\n");
    kern_handle_close(handle);
    return true;
}

/** Search for a library and then load it.
 * @param name          Name of library to load. If this contains a '/', it is
 *                      interpreted as an exact path. Otherwise, the library
 *                      search paths will be searched.
 * @param _image        Where to store pointer to image structure.
 * @return              Status code describing result of the operation. */
status_t rtld_image_load(const char *name, rtld_image_t **_image) {
    dprintf("rtld: loading image %s\n", name);

    if (!strchr(name, '/')) {
        /* Look for the library in the search paths. */
        for (size_t i = 0; library_search_dirs[i]; i++) {
            char buf[FS_PATH_MAX];

            strcpy(buf, library_search_dirs[i]);
            strcat(buf, "/");
            strcat(buf, name);

            if (path_exists(buf))
                return load_image(buf, ELF_ET_DYN, NULL, _image);
        }

        return STATUS_NOT_FOUND;
    } else {
        return load_image(name, ELF_ET_DYN, NULL, _image);
    }
}

/** Initialise the runtime loader.
 * @param _entry        Where to store program entry point address.
 * @return              Status code describing result of the operation. */
status_t rtld_init(void **_entry) {
    status_t ret;

    /* Fill in the libkernel image structure with information we have. */
    rtld_image_t *image = &libkernel_image;

    image->load_base = process_args->load_base;
    image->load_size = core_round_up((elf_addr_t)_end - (elf_addr_t)process_args->load_base, page_size);
    image->dyntab    = _DYNAMIC;

    file_info_t file;
    ret = kern_fs_info(LIBKERNEL_PATH, true, &file);
    if (ret != STATUS_SUCCESS) {
        printf("rtld: could not get information for %s\n", LIBKERNEL_PATH);
        libkernel_abort();
    }

    image->node  = file.id;
    image->mount = file.mount;

    /* Populate the dynamic table and do address fixups. */
    for (size_t i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
        if (image->dyntab[i].d_tag >= ELF_DT_NUM) {
            continue;
        } else if (image->dyntab[i].d_tag == ELF_DT_NEEDED) {
            continue;
        }

        /* Do address fixups. */
        switch (image->dyntab[i].d_tag) {
            case ELF_DT_HASH:
            case ELF_DT_PLTGOT:
            case ELF_DT_STRTAB:
            case ELF_DT_SYMTAB:
            case ELF_DT_JMPREL:
            case ELF_DT_REL_TYPE:
                image->dyntab[i].d_un.d_ptr += (elf_addr_t)process_args->load_base;
                break;
        }

        image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
    }

    /* Find out where our TLS segment is loaded to. */
    elf_ehdr_t *ehdr  = (elf_ehdr_t *)process_args->load_base;
    elf_phdr_t *phdrs = (elf_phdr_t *)(process_args->load_base + ehdr->e_phoff);

    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_TLS) {
            if (phdrs[i].p_memsz) {
                image->tls_image  = process_args->load_base + phdrs[i].p_vaddr;
                image->tls_filesz = phdrs[i].p_filesz;
                image->tls_memsz  = phdrs[i].p_memsz;
                image->tls_align  = phdrs[i].p_align;
            }

            break;
        }
    }

    rtld_symbol_init(image);

    core_list_init(&image->header);
    core_list_append(&loaded_images, &image->header);

    /* Register the image with the kernel. */
    image_info_t info;
    info.name        = image->name;
    info.path        = image->path;
    info.load_base   = image->load_base;
    info.load_size   = image->load_size;
    info.symtab      = (void *)image->dynamic[ELF_DT_SYMTAB];
    info.sym_entsize = image->dynamic[ELF_DT_SYMENT];
    info.sym_size    = image->h_nchain * info.sym_entsize;
    info.strtab      = (void *)image->dynamic[ELF_DT_STRTAB];

    ret = kern_image_register(image->id, &info);
    if (ret != STATUS_SUCCESS) {
        printf("rtld: failed to register libkernel image: %d\n", ret);
        return ret;
    }

    /* Load the program. */
    dprintf("rtld: loading program %s...\n", process_args->path);
    ret = load_image(process_args->path, ELF_ET_EXEC, _entry, &application_image);
    if (ret != STATUS_SUCCESS) {
        dprintf("rtld: failed to load binary: %d\n", ret);
        return ret;
    }

    /* We must calculate the TLS offset for the libkernel image after the
     * application has been loaded because its TLS data is positioned after the
     * application's. */
    if (image->tls_memsz)
        image->tls_offset = tls_tp_offset(image);

    /* Print out the image list if required. */
    if (libkernel_debug || libkernel_dry_run) {
        dprintf("rtld: final image list:\n");

        core_list_foreach(&loaded_images, iter) {
            rtld_image_t *loaded = core_list_entry(iter, rtld_image_t, header);

            if (loaded->path) {
                printf("  %s => %s (%p)\n", loaded->name, loaded->path, loaded->load_base);
            } else {
                printf("  %s (%p)\n", loaded->name, loaded->load_base);
            }
        }
    }

    return STATUS_SUCCESS;
}

/* TODO: Move this out of here. */
__sys_export int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data) {
    // FIXME: lock.

    core_list_foreach(&loaded_images, iter) {
        rtld_image_t *image = core_list_entry(iter, rtld_image_t, header);

        struct dl_phdr_info info;
        info.dlpi_addr  = (elf_addr_t)image->load_base;
        info.dlpi_name  = image->name;
        info.dlpi_phdr  = image->phdrs;
        info.dlpi_phnum = image->num_phdrs;

        int ret = callback(&info, sizeof(info), data);
        if (ret != 0)
            return ret;
    }

    return 0;
}
