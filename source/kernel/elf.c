/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               ELF loader.
 */

#include <io/fs.h>

#include <kernel/private/image.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/phys.h>
#include <mm/safe.h>
#include <mm/vm.h>

#include <proc/process.h>

#include <elf.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

/** Define to enable debug output from the ELF loader. */
//#define DEBUG_ELF

#ifdef DEBUG_ELF
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Next kernel image ID (protected by kernel_proc lock). */
static image_id_t next_kernel_image_id = 2;

/** Check whether an ELF header is valid for the current system.
 * @param ehdr          Executable header.
 * @return              True if valid, false if not. */
static bool check_ehdr(elf_ehdr_t *ehdr) {
    if (strncmp((const char *)ehdr->e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
        return false;
    } else if (ehdr->e_ident[ELF_EI_VERSION] != 1 || ehdr->e_version != 1) {
        return false;
    } else if (ehdr->e_ident[ELF_EI_CLASS] != ELF_CLASS) {
        return false;
    } else if (ehdr->e_ident[ELF_EI_DATA] != ELF_ENDIAN) {
        return false;
    } else if (ehdr->e_machine != ELF_MACHINE) {
        return false;
    }

    return true;
}

/** Get a section in an ELF image.
 * @param image         Image to get from.
 * @param idx           Index of section.
 * @return              Pointer to section header. */
static elf_shdr_t *get_image_section(elf_image_t *image, size_t idx) {
    return (elf_shdr_t *)((ptr_t)image->shdrs + (image->ehdr->e_shentsize * idx));
}

/**
 * Executable loader.
 */

/** Reserve space for an ELF binary in an address space.
 * @param handle        Handle to binary.
 * @param as            Address space to reserve in.
 * @return              Status code describing result of the operation. */
status_t elf_binary_reserve(object_handle_t *handle, vm_aspace_t *as) {
    elf_ehdr_t ehdr;
    size_t size, bytes, i;
    elf_phdr_t *phdrs;
    ptr_t start, end;
    status_t ret;

    /* Read the ELF header in from the file. */
    ret = file_read(handle, &ehdr, sizeof(ehdr), 0, &bytes);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (bytes != sizeof(ehdr)) {
        return STATUS_UNKNOWN_IMAGE;
    } else if (!check_ehdr(&ehdr)) {
        return STATUS_UNKNOWN_IMAGE;
    }

    /* If the binary's type is ET_DYN, we don't need to reserve space, as it can
     * be loaded to anywhere. */
    if (ehdr.e_type == ELF_ET_DYN) {
        return STATUS_SUCCESS;
    } else if (ehdr.e_type != ELF_ET_EXEC) {
        return STATUS_UNKNOWN_IMAGE;
    }

    /* Check that program headers are the right size. */
    if (ehdr.e_phentsize != sizeof(elf_phdr_t))
        return STATUS_MALFORMED_IMAGE;

    /* Allocate some memory for the program headers and load them too. */
    size = ehdr.e_phnum * ehdr.e_phentsize;
    phdrs = kmalloc(size, MM_USER);
    if (!phdrs)
        return STATUS_NO_MEMORY;

    ret = file_read(handle, phdrs, size, ehdr.e_phoff, &bytes);
    if (ret != STATUS_SUCCESS) {
        kfree(phdrs);
        return ret;
    } else if (bytes != size) {
        kfree(phdrs);
        return STATUS_MALFORMED_IMAGE;
    }

    /* Reserve space for each LOAD header. */
    for (i = 0; i < ehdr.e_phnum; i++) {
        if (phdrs[i].p_type != ELF_PT_LOAD)
            continue;

        start = round_down(phdrs[i].p_vaddr, PAGE_SIZE);
        end = round_up(phdrs[i].p_vaddr + phdrs[i].p_memsz, PAGE_SIZE);
        size = end - start;

        ret = vm_reserve(as, start, size);
        if (ret != STATUS_SUCCESS) {
            kfree(phdrs);
            return ret;
        }
    }

    kfree(phdrs);
    return STATUS_SUCCESS;
}

/** Handle an ELF_PT_LOAD program header.
 * @param image         ELF image structure.
 * @param i             Index of program header.
 * @param handle        Handle to image.
 * @param as            Address space to load into.
 * @return              Status code describing result of the operation. */
static status_t do_load_phdr(elf_image_t *image, size_t i, object_handle_t *handle, vm_aspace_t *as) {
    elf_phdr_t *phdr = &image->phdrs[i];
    uint32_t access = 0;
    ptr_t start, end;
    size_t size;
    offset_t offset;
    status_t ret;

    /* Work out the access flags to use. */
    if (phdr->p_flags & ELF_PF_R)
        access |= VM_ACCESS_READ;
    if (phdr->p_flags & ELF_PF_W)
        access |= VM_ACCESS_WRITE;
    if (phdr->p_flags & ELF_PF_X)
        access |= VM_ACCESS_EXECUTE;
    if (!access) {
        dprintf("elf: %s: program header %zu has no protection flags set\n", image->name, i);
        return STATUS_MALFORMED_IMAGE;
    }

    /* Map an anonymous region if memory size is greater than file size. */
    if (phdr->p_memsz > phdr->p_filesz) {
        start = image->load_base + round_down(phdr->p_vaddr + phdr->p_filesz, PAGE_SIZE);
        end = image->load_base + round_up(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        size = end - start;

        dprintf("elf: %s: loading BSS for %zu to %p (size: %zu)\n", image->name, i, start, size);

        /* We have to have it writeable for us to be able to clear it later on. */
        if (!(access & VM_ACCESS_WRITE)) {
            dprintf("elf: %s: program header %zu should be writeable\n", image->name, i);
            return STATUS_MALFORMED_IMAGE;
        }

        /* Create an anonymous memory region for it. */
        ret = vm_map(as, &start, size, 0, VM_ADDRESS_EXACT, access, VM_MAP_PRIVATE, NULL, 0, NULL);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    /* If file size is zero then this header is just uninitialized data. */
    if (phdr->p_filesz == 0)
        return STATUS_SUCCESS;

    /* Work out the address to map to and the offset in the file. */
    start = image->load_base + round_down(phdr->p_vaddr, PAGE_SIZE);
    end = image->load_base + round_up(phdr->p_vaddr + phdr->p_filesz, PAGE_SIZE);
    size = end - start;
    offset = round_down(phdr->p_offset, PAGE_SIZE);

    dprintf("elf: %s: loading program header %zu to %p (size: %zu)\n", image->name, i, start, size);

    /* Map the data in. Set the private flag if mapping as writeable. We do not
     * need to check whether the supplied addresses are valid - vm_map() will
     * reject them if they aren't. */
    return vm_map(
        as, &start, size, 0, VM_ADDRESS_EXACT, access,
        (access & VM_ACCESS_WRITE) ? VM_MAP_PRIVATE : 0,
        handle, offset, NULL);
}

/** Load an ELF binary into an address space.
 * @param handle        Handle to file being loaded.
 * @param path          Path to binary (used to name regions).
 * @param as            Address space to load into.
 * @param _image        Where to store image to pass to elf_binary_finish().
 * @return              Status code describing result of the operation. */
status_t elf_binary_load(
    object_handle_t *handle, const char *path, vm_aspace_t *as,
    elf_image_t **_image)
{
    elf_image_t *image;
    size_t bytes, i, size, load_count = 0;
    status_t ret;

    image = kmalloc(sizeof(*image), MM_KERNEL);
    image->name = kbasename(path, MM_KERNEL);
    image->ehdr = kmalloc(sizeof(*image->ehdr), MM_KERNEL);
    image->phdrs = NULL;

    ret = file_read(handle, image->ehdr, sizeof(*image->ehdr), 0, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != sizeof(*image->ehdr)) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (!check_ehdr(image->ehdr)) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    }

    /* Ensure that it is a type that we can load. */
    if (image->ehdr->e_type != ELF_ET_EXEC && image->ehdr->e_type != ELF_ET_DYN) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    }

    /* Check that program headers are the right size. */
    if (image->ehdr->e_phentsize != sizeof(elf_phdr_t)) {
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* Allocate some memory for the program headers and load them too. */
    size = image->ehdr->e_phnum * image->ehdr->e_phentsize;
    image->phdrs = kmalloc(size, MM_KERNEL);
    ret = file_read(handle, image->phdrs, size, image->ehdr->e_phoff, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != size) {
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* If loading an ET_DYN binary, work out how much space is required and map
     * a chunk into the address space for it. */
    if (image->ehdr->e_type == ELF_ET_DYN) {
        for (i = 0, image->load_size = 0; i < image->ehdr->e_phnum; i++) {
            if (image->phdrs[i].p_type != ELF_PT_LOAD)
                continue;

            if ((image->phdrs[i].p_vaddr + image->phdrs[i].p_memsz) > image->load_size) {
                image->load_size = round_up(
                    image->phdrs[i].p_vaddr + image->phdrs[i].p_memsz,
                    PAGE_SIZE);
            }
        }

        /* If a location is specified, force the binary to be there. */
        ret = vm_map(
            as, &image->load_base, image->load_size, 0, VM_ADDRESS_ANY,
            VM_ACCESS_READ, VM_MAP_PRIVATE, NULL, 0, NULL);
        if (ret != STATUS_SUCCESS)
            goto fail;
    } else {
        image->load_base = 0;
        image->load_size = 0;
    }

    /* Handle all the program headers. */
    for (i = 0; i < image->ehdr->e_phnum; i++) {
        switch (image->phdrs[i].p_type) {
        case ELF_PT_LOAD:
            ret = do_load_phdr(image, i, handle, as);
            if (ret != STATUS_SUCCESS)
                goto fail;

            load_count++;
            break;
        case ELF_PT_TLS:
            /* This is handled internally by libkernel, so allow it. */
            break;
        case ELF_PT_DYNAMIC:
        case ELF_PT_PHDR:
        case ELF_PT_NOTE:
            /* These can be ignored without warning. */
            break;
        case ELF_PT_INTERP:
            /* This code is used to load the kernel library, which must not have
             * an interpreter. */
            kprintf(LOG_WARN, "elf: %s: unexpected PT_INTERP header\n", image->name);
            ret = STATUS_NOT_SUPPORTED;
            goto fail;
        case ELF_PT_GNU_EH_FRAME:
        case ELF_PT_GNU_STACK:
            // FIXME: Handle stack. Need to take into account binary flags as
            // well, and library ones, so RTLD should do something.
            break;
        default:
            kprintf(
                LOG_WARN, "elf: %s: unhandled program header type %u\n",
                image->name, image->phdrs[i].p_type);

            ret = STATUS_NOT_SUPPORTED;
            goto fail;
        }
    }

    /* Check if we actually loaded anything. */
    if (!load_count) {
        kprintf(LOG_WARN, "elf: %s: no loadable program headers\n", image->name);
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    *_image = image;
    return STATUS_SUCCESS;

fail:
    kfree(image->phdrs);
    kfree(image->ehdr);
    kfree(image->name);
    kfree(image);
    return ret;
}

/** Finish binary loading, after address space is switched.
 * @param image         ELF image structure from elf_binary_load().
 * @return              Address of entry point. */
ptr_t elf_binary_finish(elf_image_t *image) {
    size_t i;
    ptr_t ret, base;

    /* Clear the BSS sections. */
    for (i = 0; i < image->ehdr->e_phnum; i++) {
        switch (image->phdrs[i].p_type) {
        case ELF_PT_LOAD:
            if (image->phdrs[i].p_filesz >= image->phdrs[i].p_memsz)
                break;

            base = image->load_base + image->phdrs[i].p_vaddr + image->phdrs[i].p_filesz;

            dprintf("elf: clearing BSS for program header %zu at %p\n", i, base);

            memset((void *)base, 0, image->phdrs[i].p_memsz - image->phdrs[i].p_filesz);
            break;
        }
    }

    ret = image->load_base + image->ehdr->e_entry;

    /* We don't add this to the process' image list. It is the resposibility of
     * libkernel to register itself. */
    kfree(image->phdrs);
    kfree(image->ehdr);
    kfree(image->name);
    kfree(image);
    return ret;
}

/**
 * Kernel module loader.
 */

/** Resolve a symbol in a module.
 * @param image         Image to get value from.
 * @param num           Number of the symbol.
 * @param _val          Where to store symbol value.
 * @return              Status code describing result of the operation. */
status_t elf_module_resolve(elf_image_t *image, size_t num, elf_addr_t *_val) {
    elf_sym_t *sym;
    const char *name;
    symbol_t ksym;

    if (num >= (image->sym_size / image->sym_entsize))
        return STATUS_MALFORMED_IMAGE;

    sym = (elf_sym_t *)(image->symtab + (image->sym_entsize * num));
    if (sym->st_shndx == ELF_SHN_UNDEF) {
        /* External symbol, look up in the kernel and other modules. */
        name = (const char *)image->strtab + sym->st_name;
        if (!symbol_lookup(name, true, true, &ksym)) {
            kprintf(LOG_WARN, "elf: %s: reference to undefined symbol `%s'\n", image->name, name);
            return STATUS_MISSING_SYMBOL;
        }

        *_val = ksym.addr;
    } else {
        /* Internal symbol. */
        *_val = sym->st_value + image->load_base;
    }

    return STATUS_SUCCESS;
}

/** Allocate memory for all loadable sections and load them.
 * @param image         Image being loaded.
 * @param handle        Handle to image.
 * @return              Status code describing result of the operation. */
static status_t load_module_sections(elf_image_t *image, object_handle_t *handle) {
    elf_shdr_t *shdr;
    size_t i, bytes;
    ptr_t dest;
    status_t ret;

    /* Calculate the total size. */
    image->load_size = 0;
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        switch (shdr->sh_type) {
        case ELF_SHT_PROGBITS:
        case ELF_SHT_NOBITS:
        case ELF_SHT_STRTAB:
        case ELF_SHT_SYMTAB:
            if (shdr->sh_addralign)
                image->load_size = round_up(image->load_size, shdr->sh_addralign);

            image->load_size += shdr->sh_size;
            break;
        }
    }

    if (!image->load_size) {
        kprintf(LOG_WARN, "elf: %s: no loadable sections\n", image->name);
        return STATUS_MALFORMED_IMAGE;
    }

    /* Allocate space to load the module into. */
    dest = image->load_base = module_mem_alloc(image->load_size);
    if (!dest)
        return STATUS_NO_MEMORY;

    /* For each section, read its data into the allocated area. */
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        switch (shdr->sh_type) {
        case ELF_SHT_NOBITS:
            if (shdr->sh_addralign)
                dest = round_up(dest, shdr->sh_addralign);

            shdr->sh_addr = dest;

            dprintf(
                "elf: %s: clearing SHT_NOBITS section %u at %p (size: %u)\n",
                image->name, i, dest, shdr->sh_size);

            memset((void *)dest, 0, shdr->sh_size);
            dest += shdr->sh_size;
            break;
        case ELF_SHT_PROGBITS:
        case ELF_SHT_STRTAB:
        case ELF_SHT_SYMTAB:
            if (shdr->sh_addralign)
                dest = round_up(dest, shdr->sh_addralign);

            shdr->sh_addr = (elf_addr_t)dest;

            dprintf(
                "elf: %s: loading data for section %u to %p (size: %u, type: %u)\n",
                image->name, i, dest, shdr->sh_size, shdr->sh_type);

            /* Read the section data in. */
            ret = file_read(handle, (void *)dest, shdr->sh_size, shdr->sh_offset, &bytes);
            if (ret != STATUS_SUCCESS) {
                return ret;
            } else if (bytes != shdr->sh_size) {
                return STATUS_MALFORMED_IMAGE;
            }

            dest += shdr->sh_size;
            break;
        case ELF_SHT_REL:
        case ELF_SHT_RELA:
            /* Read in the relocations to a temporary location. They will be
             * freed later on. */
            shdr->sh_addr = (ptr_t)kmalloc(shdr->sh_size, MM_KERNEL);

            ret = file_read(handle, (void *)shdr->sh_addr, shdr->sh_size, shdr->sh_offset, &bytes);
            if (ret != STATUS_SUCCESS) {
                return ret;
            } else if (bytes != shdr->sh_size) {
                return STATUS_MALFORMED_IMAGE;
            }

            break;
        }
    }

    return STATUS_SUCCESS;
}

/** Fix up symbols in an ELF module.
 * @param image         ELF image structure.
 * @return              Status code describing result of the operation. */
static status_t fix_module_symbols(elf_image_t *image) {
    size_t i;
    elf_shdr_t *shdr;
    elf_sym_t *sym;

    /* Look for the symbol and string tables. */
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        if (shdr->sh_type == ELF_SHT_SYMTAB) {
            image->symtab = (void *)shdr->sh_addr;
            image->sym_size = shdr->sh_size;
            image->sym_entsize = shdr->sh_entsize;
            shdr = get_image_section(image, shdr->sh_link);
            image->strtab = (void *)shdr->sh_addr;
            break;
        }
    }

    if (i == image->ehdr->e_shnum) {
        dprintf("elf: %s: could not find symbol table\n", image->name);
        return STATUS_MALFORMED_IMAGE;
    }

    for (i = 0; i < image->sym_size / image->sym_entsize; i++) {
        sym = (elf_sym_t *)(image->symtab + (image->sym_entsize * i));
        if (sym->st_shndx == ELF_SHN_UNDEF || sym->st_shndx > image->ehdr->e_shnum)
            continue;

        /* Get the section that the symbol corresponds to. */
        shdr = get_image_section(image, sym->st_shndx);
        if ((shdr->sh_flags & ELF_SHF_ALLOC) == 0)
            continue;

        /* Fix up the symbol value. Symbol value should be the symbol's offset
         * from the module's load base. */
        sym->st_value = sym->st_value + shdr->sh_addr - image->load_base;
    }

    return STATUS_SUCCESS;
}

/** Perform REL relocations on a module.
 * @param image         Image to relocate.
 * @param shdr          Relocation section.
 * @param target        Target section.
 * @return              Status code describing result of the operation. */
static status_t apply_rel_relocs(elf_image_t *image, elf_shdr_t *shdr, elf_shdr_t *target) {
    size_t i;
    elf_rel_t *rel;
    status_t ret;

    for (i = 0; i < (shdr->sh_size / shdr->sh_entsize); i++) {
        rel = (elf_rel_t *)(shdr->sh_addr + (i * shdr->sh_entsize));

        ret = arch_elf_module_relocate_rel(image, rel, target);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/** Perform RELA relocations on a module.
 * @param image         Image to relocate.
 * @param shdr          Relocation section.
 * @param target        Target section.
 * @return              Status code describing result of the operation. */
static status_t apply_rela_relocs(elf_image_t *image, elf_shdr_t *shdr, elf_shdr_t *target) {
    size_t i;
    elf_rela_t *rel;
    status_t ret;

    for (i = 0; i < (shdr->sh_size / shdr->sh_entsize); i++) {
        rel = (elf_rela_t *)(shdr->sh_addr + (i * shdr->sh_entsize));

        ret = arch_elf_module_relocate_rela(image, rel, target);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    return STATUS_SUCCESS;
}

/** Perform relocations on a module.
 * @param image         Image to relocate.
 * @param info          Whether to relocate module info sections.
 * @return              Status code describing result of the operation. */
static status_t relocate_module(elf_image_t *image, bool info) {
    elf_shdr_t *strtab, *shdr, *target;
    const char *name;
    status_t ret;
    size_t i;

    /* Need the string table for section names. */
    strtab = get_image_section(image, image->ehdr->e_shstrndx);

    /* Look for relocation sections in the module. */
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        if (shdr->sh_type != ELF_SHT_REL && shdr->sh_type != ELF_SHT_RELA)
            continue;

        /* Check whether the target is a section we want to relocate. */
        target = get_image_section(image, shdr->sh_info);
        name = (const char *)strtab->sh_addr + target->sh_name;
        if (info) {
            if (strcmp(name, MODULE_INFO_SECTION) != 0)
                continue;
        } else {
            if (strcmp(name, MODULE_INFO_SECTION) == 0)
                continue;
        }

        dprintf(
            "elf: %s: performing REL%s relocations in section %zu\n",
            image->name, (shdr->sh_type == ELF_SHT_RELA) ? "A" : "", i);

        /* Perform the relocation. */
        ret = (shdr->sh_type == ELF_SHT_RELA)
            ? apply_rela_relocs(image, shdr, target)
            : apply_rel_relocs(image, shdr, target);
        if (ret != STATUS_SUCCESS)
            return ret;

        /* Free up the relocations, they're in a temporary allocation. */
        kfree((void *)shdr->sh_addr);
        shdr->sh_addr = 0;
    }

    return STATUS_SUCCESS;

}

/** Load an ELF kernel module.
 * @param handle        Handle to the module to load.
 * @param path          Path to the module file.
 * @param image         ELF image structure for the module.
 * @return              Status code describing result of the operation. */
status_t elf_module_load(object_handle_t *handle, const char *path, elf_image_t *image) {
    size_t size, bytes, i;
    elf_shdr_t *shdr;
    status_t ret;

    memset(image, 0, sizeof(*image));
    list_init(&image->header);
    image->name = kbasename(path, MM_KERNEL);
    image->ehdr = kmalloc(sizeof(*image->ehdr), MM_KERNEL);

    /* Read the ELF header in from the file. */
    ret = file_read(handle, image->ehdr, sizeof(*image->ehdr), 0, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != sizeof(*image->ehdr)) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (!check_ehdr(image->ehdr)) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    } else if (image->ehdr->e_type != ELF_ET_REL) {
        ret = STATUS_UNKNOWN_IMAGE;
        goto fail;
    }

    /* Calculate the size of the section headers and allocate space. */
    size = image->ehdr->e_shnum * image->ehdr->e_shentsize;
    image->shdrs = kmalloc(size, MM_KERNEL);

    /* Read the headers in. */
    ret = file_read(handle, image->shdrs, size, image->ehdr->e_shoff, &bytes);
    if (ret != STATUS_SUCCESS) {
        goto fail;
    } else if (bytes != size) {
        ret = STATUS_MALFORMED_IMAGE;
        goto fail;
    }

    /* Load all loadable sections into memory. */
    ret = load_module_sections(image, handle);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* Fix up the symbol table. */
    ret = fix_module_symbols(image);
    if (ret != STATUS_SUCCESS)
        goto fail;

    /* Finally relocate the module information sections. We do not want to fully
     * relocate the module at this time as the module loader needs to check its
     * dependencies first. */
    ret = relocate_module(image, true);
    if (ret != STATUS_SUCCESS)
        goto fail;

    return STATUS_SUCCESS;

fail:
    if (image->load_base) {
        module_mem_free(image->load_base, image->load_size);

        /* Free up allocations made for relocations. */
        for (i = 0; i < image->ehdr->e_shnum; i++) {
            shdr = get_image_section(image, i);
            if (shdr->sh_type != ELF_SHT_REL && shdr->sh_type != ELF_SHT_RELA)
                continue;

            kfree((void *)shdr->sh_addr);
        }
    }

    kfree(image->shdrs);
    kfree(image->ehdr);
    kfree(image->name);
    return ret;
}

/** Finish loading an ELF module.
 * @param image         Module to finish.
 * @return              Status code describing result of the operation. */
status_t elf_module_finish(elf_image_t *image) {
    status_t ret;

    /* Perform remaining relocations. */
    ret = relocate_module(image, false);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Register the image in the kernel process. */
    mutex_lock(&kernel_proc->lock);
    image->id = next_kernel_image_id++;
    list_append(&kernel_proc->images, &image->header);
    mutex_unlock(&kernel_proc->lock);

    return STATUS_SUCCESS;
}

/** Free up data for an ELF module.
 * @param image         Image to destroy. */
void elf_module_destroy(elf_image_t *image) {
    size_t i;
    elf_shdr_t *shdr;

    mutex_lock(&kernel_proc->lock);
    list_remove(&image->header);
    mutex_unlock(&kernel_proc->lock);

    module_mem_free(image->load_base, image->load_size);

    /* Free up allocations made for relocations. */
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        if (shdr->sh_type != ELF_SHT_REL && shdr->sh_type != ELF_SHT_RELA)
            continue;

        kfree((void *)shdr->sh_addr);
    }

    kfree(image->shdrs);
    kfree(image->ehdr);
    kfree(image->name);
}

/** Look up an image symbol by address.
 * @param image         Image to look up in.
 * @param addr          Address to lookup.
 * @param symbol        Symbol structure to fill in.
 * @param _off          Where to store symbol offset (can be NULL).
 * @return              Whether a symbol was found for the address. If a symbol
 *                      is not found, but the address lies within the image's
 *                      load region, then the image pointer in the symbol will
 *                      be set to the image, otherwise it will be set to NULL. */
bool elf_symbol_from_addr(elf_image_t *image, ptr_t addr, symbol_t *symbol, size_t *_off) {
    size_t i;
    elf_sym_t *sym;
    uint8_t type;
    ptr_t value;

    for (i = 0; i < image->sym_size / image->sym_entsize; i++) {
        sym = (elf_sym_t *)(image->symtab + (image->sym_entsize * i));
        if (sym->st_shndx == ELF_SHN_UNDEF)
            continue;

        /* Ignore certain symbol types. */
        type = ELF_ST_TYPE(sym->st_info);
        if (type == ELF_STT_NOTYPE || type == ELF_STT_SECTION || type == ELF_STT_FILE)
            continue;

        value = sym->st_value + image->load_base;
        if (addr >= value && addr < (value + sym->st_size)) {
            if (_off)
                *_off = addr - value;

            symbol->addr = value;
            symbol->size = sym->st_size;
            symbol->name = (const char *)image->strtab + sym->st_name;
            symbol->global = (ELF_ST_BIND(sym->st_info)) ? true : false;
            symbol->exported = ELF_ST_VISIBILITY(sym->st_other) == ELF_STV_DEFAULT;
            symbol->image = image;
            return true;
        }
    }

    if (addr >= image->load_base && addr < image->load_base + image->load_size) {
        symbol->image = image;
    } else {
        symbol->image = NULL;
    }

    return false;
}

/** Look up an image symbol by name.
 * @param image         Image to look up in.
 * @param name          Name to lookup.
 * @param global        Whether to only look up global symbols.
 * @param exported      Whether to only look up exported symbols.
 * @param symbol        Symbol structure to fill in.
 * @return              Whether a symbol by this name was found. */
bool elf_symbol_lookup(
    elf_image_t *image, const char *name, bool global, bool exported,
    symbol_t *symbol)
{
    elf_shdr_t *shdr;
    size_t i;
    elf_sym_t *sym;
    uint8_t type;

    for (i = 0; i < image->sym_size / image->sym_entsize; i++) {
        sym = (elf_sym_t *)(image->symtab + (image->sym_entsize * i));
        if (sym->st_shndx == ELF_SHN_UNDEF)
            continue;

        /* Ignore certain symbol types. */
        type = ELF_ST_TYPE(sym->st_info);
        if (type == ELF_STT_NOTYPE || type == ELF_STT_SECTION || type == ELF_STT_FILE)
            continue;

        /* Ignore symbols in unallocated sections. */
        shdr = get_image_section(image, sym->st_shndx);
        if (!(shdr->sh_flags & ELF_SHF_ALLOC))
            continue;

        if (strcmp((const char *)image->strtab + sym->st_name, name) == 0) {
            if (global && !ELF_ST_BIND(sym->st_info)) {
                continue;
            } else if (exported && ELF_ST_VISIBILITY(sym->st_other) != ELF_STV_DEFAULT) {
                continue;
            }

            symbol->addr = sym->st_value + image->load_base;
            symbol->size = sym->st_size;
            symbol->name = (const char *)image->strtab + sym->st_name;
            symbol->global = (ELF_ST_BIND(sym->st_info)) ? true : false;
            symbol->exported = ELF_ST_VISIBILITY(sym->st_other) == ELF_STV_DEFAULT;
            symbol->image = image;
            return true;
        }
    }

    return false;
}

/** Print a list of loaded images in a process.
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @return              KDB status code. */
static kdb_status_t kdb_cmd_images(int argc, char **argv, kdb_filter_t *filter) {
    uint64_t val;
    process_t *process;
    elf_image_t *image;

    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s <process ID>\n\n", argv[0]);

        kdb_printf("Prints a list of all loaded images in a process.\n");
        return KDB_SUCCESS;
    } else if (argc != 2) {
        kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    if (kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    process = process_lookup_unsafe(val);
    if (!process) {
        kdb_printf("Invalid process ID.\n");
        return KDB_FAILURE;
    }

    kdb_printf("ID     Base               Size       Name\n");
    kdb_printf("==     ====               ====       ====\n");

    list_foreach(&process->images, iter) {
        image = list_entry(iter, elf_image_t, header);

        kdb_printf(
            "%-6" PRIu16 " %-18p 0x%-8zx %s\n",
            image->id, image->load_base, image->load_size, image->name);
    }

    return KDB_SUCCESS;
}

/** Initialize the kernel ELF information.
 * @param image         Kernel ELF image. */
__init_text void elf_init(elf_image_t *image) {
    kboot_tag_sections_t *sections;
    uint32_t i;
    elf_shdr_t *shdr;
    void *mapping;

    list_init(&image->header);
    image->id = 1;
    image->name = (char *)"kernel";
    image->load_base = 0;
    image->load_size = 0;

    /* Find the loaded section information for the kernel. */
    sections = kboot_tag_iterate(KBOOT_TAG_SECTIONS, NULL);
    if (!sections || !sections->num)
        fatal("No kernel section information provided");

    kprintf(
        LOG_DEBUG,
        "elf: kernel has %" PRIu32 " section headers (shentsize: %" PRIu32 ", shstrndx: %" PRIu32 ")\n",
        sections->num, sections->entsize, sections->shstrndx);

    /* KBoot gives us physical addresses of the sections. Go through and map
     * them into virtual memory. */
    for (i = 0; i < sections->num; i++) {
        shdr = (elf_shdr_t *)&sections->sections[i * sections->entsize];

        if (shdr->sh_flags & ELF_SHF_ALLOC ||
            !shdr->sh_size ||
            (shdr->sh_type != ELF_SHT_PROGBITS &&
                shdr->sh_type != ELF_SHT_NOBITS &&
                shdr->sh_type != ELF_SHT_SYMTAB &&
                shdr->sh_type != ELF_SHT_STRTAB))
        {
            continue;
        }

        mapping = phys_map(shdr->sh_addr, shdr->sh_size, MM_BOOT);

        kprintf(LOG_DEBUG, "elf: mapped section %u: 0x%lx -> %p\n", i, shdr->sh_addr, mapping);

        shdr->sh_addr = (ptr_t)mapping;
    }

    /* The executable header is at the start of the kernel image. */
    image->ehdr = kmemdup((elf_ehdr_t *)KERNEL_VIRT_BASE, sizeof(*image->ehdr), MM_BOOT);
    image->shdrs = kmemdup(sections->sections, sections->num * sections->entsize, MM_BOOT);

    /* Look for the symbol and string tables. */
    for (i = 0; i < image->ehdr->e_shnum; i++) {
        shdr = get_image_section(image, i);
        if (shdr->sh_type == ELF_SHT_SYMTAB) {
            image->symtab = (void *)shdr->sh_addr;
            image->sym_size = shdr->sh_size;
            image->sym_entsize = shdr->sh_entsize;
            shdr = get_image_section(image, shdr->sh_link);
            image->strtab = (void *)shdr->sh_addr;
            break;
        }
    }

    if (i == image->ehdr->e_shnum)
        fatal("Could not find kernel symbol table");

    /* Register the KDB command. */
    kdb_register_command(
        "images", "Display information about a process' loaded images.",
        kdb_cmd_images);
}

/**
 * User image management.
 */

/** Clone loaded image information.
 * @param process       New process.
 * @param parent        Parent process. */
void elf_process_clone(process_t *process, process_t *parent) {
    elf_image_t *image, *clone;

    list_foreach(&parent->images, iter) {
        image = list_entry(iter, elf_image_t, header);

        clone = kmemdup(image, sizeof(*image), MM_KERNEL);
        clone->name = kstrdup(image->name, MM_KERNEL);
        list_init(&clone->header);
        list_append(&process->images, &clone->header);
    }
}

/** Clean up ELF images attached to a process.
 * @param process       Process to clean up. */
void elf_process_cleanup(process_t *process) {
    elf_image_t *image;

    list_foreach_safe(&process->images, iter) {
        image = list_entry(iter, elf_image_t, header);

        list_remove(&image->header);
        kfree(image->name);
        kfree(image);
    }
}

/** Register an ELF image with the kernel.
 * @param id            Image ID.
 * @param info          Image information structure.
 * @return              Status code describing the result of the operation. */
status_t kern_image_register(image_id_t id, image_info_t *info) {
    image_info_t kinfo;
    elf_image_t *image, *exist;
    char *name;
    status_t ret;

    if (!info)
        return STATUS_INVALID_ARG;

    ret = memcpy_from_user(&kinfo, info, sizeof(kinfo));
    if (ret != STATUS_SUCCESS)
        return ret;

    if (kinfo.load_base && !is_user_range(kinfo.load_base, kinfo.load_size)) {
        return STATUS_INVALID_ADDR;
    } else if (kinfo.symtab && !is_user_range(kinfo.symtab, kinfo.sym_size)) {
        return STATUS_INVALID_ADDR;
    } else if (kinfo.strtab && !is_user_address(kinfo.strtab)) {
        return STATUS_INVALID_ADDR;
    }

    image = kmalloc(sizeof(*image), MM_KERNEL);
    list_init(&image->header);
    image->id = id;
    image->load_base = (ptr_t)kinfo.load_base;
    image->load_size = kinfo.load_size;
    image->symtab = kinfo.symtab;
    image->sym_size = kinfo.sym_size;
    image->sym_entsize = kinfo.sym_entsize;
    image->strtab = kinfo.strtab;

    ret = strndup_from_user(kinfo.name, FS_PATH_MAX, &name);
    if (ret != STATUS_SUCCESS) {
        kfree(image);
        return ret;
    }

    image->name = kbasename(name, MM_KERNEL);
    kfree(name);

    mutex_lock(&curr_proc->lock);

    list_foreach(&curr_proc->images, iter) {
        exist = list_entry(iter, elf_image_t, header);

        if (exist->id == id) {
            mutex_unlock(&curr_proc->lock);
            kfree(image->name);
            kfree(image);
            return STATUS_ALREADY_EXISTS;
        }
    }

    list_append(&curr_proc->images, &image->header);

    mutex_unlock(&curr_proc->lock);

    dprintf(
        "elf: registered image %" PRIu16 " (%s) in process %" PRIu32 " (load_base: %p, load_size: 0x%zx)\n",
        image->id, image->name, curr_proc->id, image->load_base, image->load_size);

    return STATUS_SUCCESS;
}

/** Unregister an ELF image.
 * @param id            ID of the image to unregister.
 * @return              Status code describing the result of the operation. */
status_t kern_image_unregister(image_id_t id) {
    elf_image_t *image;

    mutex_lock(&curr_proc->lock);

    list_foreach(&curr_proc->images, iter) {
        image = list_entry(iter, elf_image_t, header);

        if (image->id == id) {
            list_remove(&image->header);

            dprintf(
                "elf: unregistered image %" PRIu16 " (%s) in process %" PRIu32 "\n",
                image->id, image->name, curr_proc->id);

            kfree(image->name);
            kfree(image);

            mutex_unlock(&curr_proc->lock);
            return STATUS_SUCCESS;
        }
    }

    mutex_unlock(&curr_proc->lock);
    return STATUS_NOT_FOUND;
}
