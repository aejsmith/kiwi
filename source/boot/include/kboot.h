/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		KBoot boot format definitions.
 *
 * This file defines the KBoot boot format, which is used to load the Kiwi
 * kernel. KBoot uses two sets of tags: image tags (itags) and information tags.
 * Image tags are contained inside a kernel image in ELF note sections, and
 * influence how the image is loaded. Information tags are passed to the kernel
 * to provide information on the environment it is running in and how it was
 * loaded.
 */

#ifndef __KBOOT_H
#define __KBOOT_H

#include <types.h>

/** KBoot information tag header structure. */
typedef struct kboot_tag {
	phys_ptr_t next;			/**< Address of next tag, 0 if last tag. */
	uint32_t type;				/**< Type of the tag. */
} __packed kboot_tag_t;

/** Possible information tag types. */
#define KBOOT_TAG_OPTION		0	/**< Kernel option. */
#define KBOOT_TAG_MEMORY		1	/**< Physical memory range (at least 1 free range required). */
#define KBOOT_TAG_MODULE		2	/**< Boot module. */
#define KBOOT_TAG_BOOTDEV		3	/**< Boot device information. */
#define KBOOT_TAG_LFB			4	/**< Linear framebuffer information. */

/** Maximum length of an option name. */
#define KBOOT_OPTION_NAME_LEN		32

/** Tag containing an option passed to the kernel. */
typedef struct kboot_tag_option {
	kboot_tag_t header;			/**< Tag header. */

	char name[KBOOT_OPTION_NAME_LEN];	/**< Name of the option. */
	uint32_t type;				/**< Type of the option. */
	uint32_t size;				/**< Size of the option data following the tag. */
} __packed kboot_tag_option_t;

/** Possible option types. */
#define KBOOT_OPTION_BOOLEAN		0	/**< Boolean. */
#define KBOOT_OPTION_STRING		1	/**< String. */
#define KBOOT_OPTION_INTEGER		2	/**< Integer. */

/** Tag describing a physical memory range. */
typedef struct kboot_tag_memory {
	kboot_tag_t header;			/**< Tag header. */

	phys_ptr_t start;			/**< Start of the memory range. */
	phys_ptr_t end;				/**< End of the memory range. */
	uint32_t type;				/**< Type of the memory range. */
} __packed kboot_tag_memory_t;

/** Possible memory range types. */
#define KBOOT_MEMORY_FREE		0	/**< Free, usable memory. */
#define KBOOT_MEMORY_ALLOCATED		1	/**< Allocated memory. */
#define KBOOT_MEMORY_RECLAIMABLE	2	/**< Memory reclaimable when boot information is no longer needed. */
#define KBOOT_MEMORY_RESERVED		3	/**< Reserved memory, never usable. */

/** Tag describing a boot module. */
typedef struct kboot_tag_module {
	kboot_tag_t header;			/**< Tag header. */

	phys_ptr_t base;			/**< Address of the module. */
	uint32_t size;				/**< Size of the module. */
} __packed kboot_tag_module_t;

/** Length of the UUID field in the kboot_tag_bootdev structure. */
#define KBOOT_BOOTDEV_UUID_LEN	64

/** Tag containing boot device information. */
typedef struct kboot_tag_bootdev {
	kboot_tag_t header;			/**< Tag header. */

	char uuid[KBOOT_BOOTDEV_UUID_LEN];	/**< UUID of the boot filesystem. */
} __packed kboot_tag_bootdev_t;

/** Tag containing linear framebuffer information. */
typedef struct kboot_tag_lfb {
	kboot_tag_t header;			/**< Tag header. */

	uint16_t width;				/**< Width of the display. */
	uint16_t height;			/**< Height of the display. */
	uint8_t depth;				/**< Bits per pixel. */
	phys_ptr_t addr;			/**< Physical address of the framebuffer. */
} __packed kboot_tag_lfb_t;

/** KBoot ELF note name. */
#define KBOOT_NOTE_NAME			"KBoot"

/** KBoot image tag types (used as ELF note type field). */
#define KBOOT_ITAG_IMAGE		0	/**< Basic image information (required). */
#define KBOOT_ITAG_OPTION		1	/**< Option description. */
#define KBOOT_ITAG_MAPPING		2	/**< Virtual memory mapping description. */

/** Length of fields in the kboot_itag_image structure. */
#define KBOOT_IMAGE_NAME_LEN		32

/** Flags for the kboot_itag_image structure. */
#define KBOOT_IMAGE_LFB			(1<<0)	/**< Switch to a video mode and provide LFB information. */

/** Image tag containing basic image information. */
typedef struct kboot_itag_image {
	char name[KBOOT_IMAGE_NAME_LEN];	/**< Name of the image. */
	uint32_t flags;				/**< Flags for the image. */
} __packed kboot_itag_image_t;

/** Image tag containing an option description. */
typedef struct kboot_itag_option {
	char name[KBOOT_OPTION_NAME_LEN];	/**< Name of the option. */
	uint32_t type;				/**< Type of the option. */

	/** Default value of the option. */
	union {
		bool boolean;
		uint64_t integer;
		char string[128];
	} defval;
} __packed kboot_itag_option_t;

/** Image tag containing a virtual memory mapping description. */
typedef struct kboot_itag_mapping {
	uint64_t virt;				/**< Virtual address to map. */
	phys_ptr_t phys;			/**< Physical address to map to. */
	uint64_t size;				/**< Size of mapping to make. */
} __packed kboot_itag_mapping_t;

#endif /* __KBOOT_H */
