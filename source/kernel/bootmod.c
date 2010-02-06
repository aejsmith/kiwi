/* Kiwi boot-time module loader
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
 * @brief		Boot-time module loader.
 */

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <io/vfs.h>

#include <mm/malloc.h>

#include <assert.h>
#include <bootmod.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>

#if CONFIG_VFS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Header for a tar file. */
typedef struct tar_header {
	char name[100];		/**< Name of entry. */
	char mode[8];		/**< Mode of entry. */
	char uid[8];		/**< User ID. */
	char gid[8];		/**< Group ID. */
	char size[12];		/**< Size of entry. */
	char mtime[12];		/**< Modification time. */
	char chksum[8];		/**< Checksum. */
	char typeflag;		/**< Type flag. */
	char linkname[100];	/**< Symbolic link name. */
	char magic[6];		/**< Magic string. */
	char version[2];	/**< TAR version. */
	char uname[32];		/**< User name. */
	char gname[32];		/**< Group name. */
	char devmajor[8];	/**< Device major. */
	char devminor[8];	/**< Device minor. */
	char prefix[155];	/**< Prefix. */
} tar_header_t;

/** Tar entry types. */
#define REGTYPE		'0'	/**< Regular file (preferred code). */
#define AREGTYPE	'\0'	/**< Regular file (alternate code). */
#define LNKTYPE		'1'	/**< Hard link. */
#define SYMTYPE		'2'	/**< Symbolic link (hard if not supported). */
#define CHRTYPE		'3'	/**< Character special. */
#define BLKTYPE		'4'	/**< Block special. */
#define DIRTYPE		'5'	/**< Directory.  */
#define FIFOTYPE	'6'	/**< Named pipe.  */
#define CONTTYPE	'7'	/**< Contiguous file. */

/** Array of boot-time modules provided by architecture/platform code. */
bootmod_t bootmod_array[BOOTMOD_MAX] __init_data;
size_t bootmod_count __init_data = 0;

/** Whether a RamFS has been mounted for the root. */
static bool bootmod_mounted_ramfs __init_data = false;

/** Look up a kernel module in the boot module array.
 * @param name		Name to look for.
 * @return		Pointer to module if found, NULL if not. */
static bootmod_t *bootmod_lookup(const char *name) {
	char *tmp;
	size_t i;

	for(i = 0; i < bootmod_count; i++) {
		if(!bootmod_array[i].name) {
			tmp = kmalloc(MODULE_NAME_MAX + 1, MM_FATAL);
			if(module_name(bootmod_array[i].node, tmp) != 0) {
				kfree(tmp);
				continue;
			}
			bootmod_array[i].name = tmp;
		}

		if(strcmp(name, bootmod_array[i].name) == 0) {
			return &bootmod_array[i];
		}
	}

	return NULL;
}

/** Extract a TAR archive to the root FS.
 * @param mod		Boot module containing archive.
 * @return		Whether the module was a TAR archive. */
static bool __init_text bootmod_load_tar(bootmod_t *mod) {
	tar_header_t *hdr;
	vfs_node_t *node;
	int64_t size;
	size_t bytes;
	void *buf;
	int ret;

	buf = hdr = kmalloc(mod->node->size, MM_FATAL);
	if(vfs_file_read(mod->node, buf, mod->node->size, 0, &bytes) != 0 || bytes != mod->node->size) {
		fatal("Could not read TAR file data");
	}

	/* Check format of module. */
	if(strncmp(hdr->magic, "ustar", 5) != 0) {
		kfree(buf);
		return false;
	}

	/* If any TAR files are loaded it means we should mount a RamFS at the
	 * root, if this has not already been done. */
	if(!bootmod_mounted_ramfs) {
		if((ret = vfs_mount(NULL, "/", "ramfs", 0)) != 0) {
			fatal("Could not mount RamFS at root (%d)", ret);
		}
		bootmod_mounted_ramfs = true;
	}

	/* Loop until we encounter two null bytes (EOF). */
	while(hdr->name[0] && hdr->name[1]) {
		if(strncmp(hdr->magic, "ustar", 5) != 0) {
			fatal("TAR file format is not correct");
		}

		/* All fields in the header are stored as ASCII - convert the
		 * size to an integer (base 8). */
		size = strtoll(hdr->size, NULL, 8);

		/* Handle the entry based on its type flag. */
		switch(hdr->typeflag) {
		case REGTYPE:
		case AREGTYPE:
			if((ret = vfs_file_create(hdr->name, &node)) != 0) {
				fatal("Failed to create regular file %s (%d)", hdr->name, ret);
			}

			if((ret = vfs_file_write(node, (void *)((ptr_t)hdr + 512), size, 0, &bytes)) != 0) {
				fatal("Failed to write file %s (%d)", hdr->name, ret);
			} else if((int64_t)bytes != size) {
				fatal("Did not write all data for file %s (%zu, %zu)", hdr->name, bytes, size);
			}

			dprintf("bootmod: extracted regular file %s (%lld bytes)\n", hdr->name, size);
			vfs_node_release(node);
			break;
		case DIRTYPE:
			if((ret = vfs_dir_create(hdr->name, NULL)) != 0) {
				fatal("Failed to create directory %s (%d)", hdr->name, ret);
			}

			dprintf("bootmod: created directory %s\n", hdr->name);
			break;
		case SYMTYPE:
			if((ret = vfs_symlink_create(hdr->name, hdr->linkname, NULL)) != 0) {
				fatal("Failed to create symbolic link %s (%d)", hdr->name, ret);
			}

			dprintf("bootmod: created symbolic link %s -> %s\n", hdr->name, hdr->linkname);
			break;
		default:
			dprintf("bootmod: unhandled type flag '%c'\n", hdr->typeflag);
			break;
		}

		/* 512 for the header, plus the file size if necessary. */
		hdr = (tar_header_t *)(ptr_t)((ptr_t)hdr + 512 + ((size != 0) ? ROUND_UP(size, 512) : 0));
	}

	kfree(buf);
	return true;
}

/** Load a kernel module provided at boot.
 * @param mod		Module to load.
 * @return		Whether the file was a kernel module. */
static bool __init_text bootmod_load_kmod(bootmod_t *mod) {
	char name[MODULE_NAME_MAX + 1];
	bootmod_t *dep;
	int ret;

	/* Try to load the module and all dependencies. */
	while(true) {
		if((ret = module_load_node(mod->node, name)) == 0) {
			return true;
		} else if(ret == -ERR_TYPE_INVAL) {
			return false;
		} else if(ret != -ERR_DEP_MISSING) {
			fatal("Could not load module %p (%d)", mod, ret);
		}

		/* Unloaded dependency, try to find it and load it. */
		if(!(dep = bootmod_lookup(name)) || !bootmod_load_kmod(dep)) {
			fatal("Dependency on '%s' which is not available", name);
		} else {
			dep->loaded = true;
		}
	}
}

/** Load all boot-time modules. */
void __init_text bootmod_load(void) {
	size_t i;

	if(!bootmod_count) {
		fatal("No modules were provided, cannot do anything!");
	}

	for(i = 0; i < bootmod_count; i++) {
		/* Ignore already loaded modules (may be already loaded due
		 * to dependency loading for another module). */
		if(!bootmod_array[i].loaded) {
			if(bootmod_load_tar(&bootmod_array[i]) || bootmod_load_kmod(&bootmod_array[i])) {
				bootmod_array[i].loaded = true;
			} else {
				fatal("Module %u has unknown format", i);
			}
		}
	}

	/* Free up all the modules. */
	for(i = 0; i < bootmod_count; i++) {
		if(bootmod_array[i].name) {
			kfree(bootmod_array[i].name);
		}
		vfs_node_release(bootmod_array[i].node);
	}
}
