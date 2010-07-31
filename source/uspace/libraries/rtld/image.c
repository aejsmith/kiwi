/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		RTLD image management.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/vm.h>

#include "args.h"
#include "image.h"
#include "utility.h"

/** List of loaded images. */
LIST_DECLARE(rtld_loaded_images);

/** Array of directories to search for libraries in. */
static const char *rtld_library_dirs[] = {
	".",
	"/system/libraries",
	NULL
};

/** Load an image into memory.
 * @param path		Path to image file.
 * @param req		Image that requires this image. This is used to work
 *			out where to place the new image in the image list.
 * @param type		Required ELF type.
 * @param entryp	Where to store entry point for binary, if type is
 *			ELF_ET_EXEC.
 * @param imagep	Where to store pointer to image structure.
 * @return		0 on success, negative error code on failure. */
int rtld_image_load(const char *path, rtld_image_t *req, int type, void **entryp, rtld_image_t **imagep) {
	rtld_image_t *image = NULL, *exist;
	ElfW(Addr) start, end;
	size_t bytes, size, i;
	ElfW(Phdr) *phdrs;
	Elf32_Word *addr;
	const char *dep;
	ElfW(Ehdr) ehdr;
	handle_t handle;
	offset_t offset;
	int ret, flags;

	/* Try to open the image. */
	if((handle = fs_file_open(path, FS_FILE_READ)) < 0) {
		return (int)handle;
	}

	/* Read in its header and ensure that it is valid. */
	if((ret = fs_file_pread(handle, &ehdr, sizeof(ehdr), 0, &bytes)) != 0) {
		goto fail;
	} else if(bytes != sizeof(ehdr)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
		printf("RTLD: %s: not a valid ELF file\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(ehdr.e_ident[4] != ELF_CLASS || ehdr.e_ident[5] != ELF_ENDIAN || ehdr.e_machine != ELF_MACHINE) {
		printf("RTLD: %s: not for the machine that this RTLD is for\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(ehdr.e_ident[6] != 1 || ehdr.e_version != 1) {
		printf("RTLD: %s: not correct version\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(ehdr.e_type != type) {
		printf("RTLD: %s: incorrect ELF file type\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(ehdr.e_phentsize != sizeof(ElfW(Phdr))) {
		printf("RTLD: %s: bad program header size\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Create a structure to track information about the image. */
	if(!(image = malloc(sizeof(rtld_image_t)))) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	}
	memset(image, 0, sizeof(rtld_image_t));

	/* Don't particularly care if we can't duplicate the path string, its
	 * not important (only for debugging purposes). */
	image->path = strdup(path);
	list_init(&image->header);

	/* Read in the program headers. Use alloca() because our malloc()
	 * implementation does not support freeing, so we'd be wasting valuable
	 * heap space. */
	size = ehdr.e_phnum * ehdr.e_phentsize;
	phdrs = alloca(size);
	if((ret = fs_file_pread(handle, phdrs, size, ehdr.e_phoff, &bytes)) != 0) {
		goto fail;
	} else if(bytes != size) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* If loading a library, find out exactly how much space we need for
	 * all the LOAD headers, and allocate a chunk of memory for them. For
	 * executables, just put the load base as 0. */
	if(ehdr.e_type == ELF_ET_DYN) {
		for(i = 0, image->load_size = 0; i < ehdr.e_phnum; i++) {
			if(phdrs[i].p_type != ELF_PT_LOAD) {
				if(phdrs[i].p_type == ELF_PT_INTERP) {
					printf("RTLD: Library %s requires an interpreter!\n", path);
					ret = -ERR_FORMAT_INVAL;
					goto fail;
				}
				continue;
			}

			if((phdrs[i].p_vaddr + phdrs[i].p_memsz) > image->load_size) {
				image->load_size = ROUND_UP(phdrs[i].p_vaddr + phdrs[i].p_memsz, PAGE_SIZE);
			}
		}

		/* Allocate a chunk of memory for it. */
		if((ret = vm_map(NULL, image->load_size, VM_MAP_READ | VM_MAP_PRIVATE, -1, 0, &image->load_base)) != 0) {
			printf("RTLD: Unable to allocate memory for %s (%d)\n", path, ret);
			goto fail;
		}
	} else {
		image->load_base = NULL;
		image->load_size = 0;
	}

	/* Load all of the LOAD headers, and save the address of the dynamic
	 * section if we find it. */
	for(i = 0; i < ehdr.e_phnum; i++) {
		if(phdrs[i].p_type == ELF_PT_DYNAMIC) {
			image->dyntab = (ElfW(Dyn) *)((ElfW(Addr))image->load_base + phdrs[i].p_vaddr);
			continue;
		} else if(phdrs[i].p_type != ELF_PT_LOAD) {
			continue;
		}

		/* Work out the flags to map with. */
		flags  = ((phdrs[i].p_flags & ELF_PF_R) ? VM_MAP_READ  : 0);
		flags |= ((phdrs[i].p_flags & ELF_PF_W) ? VM_MAP_WRITE : 0);
		flags |= ((phdrs[i].p_flags & ELF_PF_X) ? VM_MAP_EXEC  : 0);
		if(flags == 0) {
			dprintf("RTLD: Program header %zu in %s has no protection flags.\n", i, path);
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}

		/* Set the fixed flag, and the private flag if mapping as
		 * writeable. */
		flags |= VM_MAP_FIXED;
		if(phdrs[i].p_flags & ELF_PF_W) {
			flags |= VM_MAP_PRIVATE;
		}

		/* Map the BSS if required. */
		if(phdrs[i].p_memsz > phdrs[i].p_filesz) {
			start = (ElfW(Addr))image->load_base + ROUND_DOWN(phdrs[i].p_vaddr + phdrs[i].p_filesz, PAGE_SIZE);
			end = (ElfW(Addr))image->load_base + ROUND_UP(phdrs[i].p_vaddr + phdrs[i].p_memsz, PAGE_SIZE);
			size = end - start;

			/* Must be writable to be able to clear later. */
			if(!(flags & VM_MAP_WRITE)) {
				dprintf("RTLD: Program header %d (%s) should be writable!\n", i, path);
				ret = -ERR_FORMAT_INVAL;
				goto fail;
			}

			/* Create an anonymous region for it. */
			if((ret = vm_map((void *)start, size, flags, -1, 0, NULL)) != 0) {
				printf("RTLD: Unable to map %s into memory (%d) (1)\n", path, ret);
				goto fail;
			}
		}

		if(phdrs[i].p_filesz == 0) {
			continue;
		}

		/* Load the data. */
		start = (ElfW(Addr))image->load_base + ROUND_DOWN(phdrs[i].p_vaddr, PAGE_SIZE);
		end = (ElfW(Addr))image->load_base + ROUND_UP(phdrs[i].p_vaddr + phdrs[i].p_filesz, PAGE_SIZE);
		size = end - start;
		offset = ROUND_DOWN(phdrs[i].p_offset, PAGE_SIZE);

		dprintf("RTLD: Loading %u (%s) to %p (size: %d)\n", i, path, start, size);

		if((ret = vm_map((void *)start, size, flags, handle, offset, NULL)) != 0) {
			printf("RTLD: Unable to map %s into memory (%d) (2)\n", path, ret);
			goto fail;
		}

		/* Clear out BSS. */
		if(phdrs[i].p_filesz < phdrs[i].p_memsz) {
			start = (ElfW(Addr))((ElfW(Addr))image->load_base + phdrs[i].p_vaddr + phdrs[i].p_filesz);
			size = phdrs[i].p_memsz - phdrs[i].p_filesz;

			dprintf("RTLD: Clearing BSS for %u (%s) [%p,%p)\n", i, path, start, start + size);
			memset((void *)start, 0, size);
		}
	}

	/* Check that there was a DYNAMIC header. */
	if(!image->dyntab) {
		dprintf("RTLD: Library %s does not have a dynamic PHDR\n", path);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Fill in our dynamic table. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED) {
			continue;
		}

		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;

		/* Do address fixups. */
		switch(image->dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			image->dynamic[image->dyntab[i].d_tag] += (ElfW(Addr))image->load_base;
			break;
		}
	}

	/* Set name and loading state, and fill out hash information.
	 * FIXME: Use base of library path if SONAME not set. */
	if(type == ELF_ET_DYN) {
		image->name = (const char *)(image->dynamic[ELF_DT_SONAME] + image->dynamic[ELF_DT_STRTAB]);
	} else {
		image->name = "<application>";
	}
	image->state = RTLD_IMAGE_LOADING;
	if(image->dynamic[ELF_DT_HASH]) {
		addr = (Elf32_Word *)image->dynamic[ELF_DT_HASH];
		image->h_nbucket = *addr++;
		image->h_nchain  = *addr++;
		image->h_buckets = addr;
		addr += image->h_nbucket;
		image->h_chains = addr;
	}

	/* Check if the image is already loaded. */
	if(type == ELF_ET_DYN) {
		LIST_FOREACH_SAFE(&rtld_loaded_images, iter) {
			exist = list_entry(iter, rtld_image_t, header);

			if(strcmp(exist->name, image->name) != 0) {
				continue;
			} else if(exist->state == RTLD_IMAGE_LOADING) {
				printf("RTLD: Cyclic dependency on %s detected!\n", image->name);
				ret = -ERR_FORMAT_INVAL;
				goto fail;
			}

			dprintf("RTLD: Increasing reference count on %s (%p)\n", image->name, exist);
			exist->refcount++;
			if(imagep) {
				*imagep = exist;
			}

			/* Use the failure path to clean up as the library must
			 * be freed. */
			ret = 0;
			goto fail;
		}
	}

	/* Add the library into the library list before checking dependencies
	 * so that we can check if something has a cyclic dependency. */
	if(req) {
		list_add_before(&req->header, &image->header);
	} else {
		list_append(&rtld_loaded_images, &image->header);
	}

	/* Load libraries that we depend on. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag != ELF_DT_NEEDED) {
			continue;
		}

		dep = (const char *)(image->dyntab[i].d_un.d_ptr + image->dynamic[ELF_DT_STRTAB]);

		/* Don't depend on ourselves... */
		if(strcmp(dep, image->name) == 0) {
			printf("RTLD: Image %s depends on itself!\n", path);
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}

		dprintf("RTLD: Image %s depends on %s, loading...\n", path, dep);
		if((ret = rtld_library_load(dep, image, NULL)) != 1) {
			if(ret == 0) {
				printf("RTLD: Could not find required library: %s\n", dep);
				ret = -ERR_DEP_MISSING;
			}
			goto fail;
		}
	}

	/* We can now perform relocations. */
	if((ret = rtld_image_relocate(image)) != 0) {
		goto fail;
	}

	image->refcount = 1;
	image->state = RTLD_IMAGE_LOADED;
	if(entryp) {
		*entryp = (void *)ehdr.e_entry;
	}
	if(imagep) {
		*imagep = image;
	}
	handle_close(handle);
	return 0;
fail:
	if(image) {
		if(image->load_base) {
			vm_unmap(image->load_base, image->load_size);
		}
		list_remove(&image->header);
		free(image);
	}
	handle_close(handle);
	return ret;
}

/** Unload an image from memory.
 * @param image		Image to unload. */
void rtld_image_unload(rtld_image_t *image) {
	void (*func)(void);
	rtld_image_t *dep;
	const char *name;
	size_t i;

	if(--image->refcount > 0) {
		dprintf("RTLD: Decreased reference count of %p(%s)\n", image, image->name);
		return;
	}

	/* Call the FINI function. */
	if(image->dynamic[ELF_DT_FINI]) {
		func = (void (*)(void))(image->load_base + image->dynamic[ELF_DT_FINI]);
		dprintf("RTLD: Calling FINI function %p...\n", func);
		func();
	}

	/* Unload all dependencies of the library. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag != ELF_DT_NEEDED) {
			continue;
		}

		name = (const char *)(image->dyntab[i].d_un.d_ptr + image->dynamic[ELF_DT_STRTAB]);
		LIST_FOREACH(&rtld_loaded_images, iter) {
			dep = list_entry(iter, rtld_image_t, header);

			if(strcmp(dep->name, name) == 0) {
				rtld_image_unload(dep);
			}
		}
	}

	dprintf("RTLD: Unloaded image %p(%s)\n", image, image->name);
	if(image->load_base) {
		vm_unmap(image->load_base, image->load_size);
	}
	list_remove(&image->header);
	free(image);
}

/** Check if a library exists.
 * @param path		Path to check.
 * @return		True if exists, false if not. */
static bool rtld_library_exists(const char *path) {
	handle_t handle;

	dprintf("  Trying %s... ", path);

	/* Attempt to open it to see if it is there. */
	if((handle = fs_file_open(path, FS_FILE_READ)) < 0) {
		dprintf("returned %d\n", handle);
		return false;
	}

	dprintf("success!\n");
	handle_close(handle);
	return true;
}

/** Search for a library and then load it.
 * @todo		Use PATH_MAX for buffer size.
 * @param name		Name of library to load.
 * @param req		Image that requires this library.
 * @param imagep	Where to store pointer to image structure.
 * @return		1 on success, 0 if the library is missing, negative
 *                      error code on failure. */
int rtld_library_load(const char *name, rtld_image_t *req, rtld_image_t **imagep) {
	char buf[4096];
	size_t i;
	int ret;

	/* Look for the library in the search paths. */
	for(i = 0; rtld_extra_libpaths[i]; i++) {
		strcpy(buf, rtld_extra_libpaths[i]);
		strcat(buf, "/");
		strcat(buf, name);
		if(rtld_library_exists(buf)) {
			ret = rtld_image_load(buf, req, ELF_ET_DYN, NULL, imagep);
			return (ret < 0) ? ret : 1;
		}
	}
	for(i = 0; rtld_library_dirs[i]; i++) {
		strcpy(buf, rtld_library_dirs[i]);
		strcat(buf, "/");
		strcat(buf, name);
		if(rtld_library_exists(buf)) {
			ret = rtld_image_load(buf, req, ELF_ET_DYN, NULL, imagep);
			return (ret < 0) ? ret : 1;
		}
	}

	return 0;
}
