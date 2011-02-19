/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Low Resource Manager.
 */

#ifndef __LRM_H
#define __LRM_H

#include <lib/list.h>

/** Resource types. */
#define RESOURCE_TYPE_MEMORY	(1<<0)	/**< Physical memory. */
#define RESOURCE_TYPE_KASPACE	(1<<1)	/**< Kernel address space. */

/** Resource levels. */
#define RESOURCE_LEVEL_OK	0	/**< Within acceptable level. */
#define RESOURCE_LEVEL_ADVISORY	1	/**< Nearing low level, advise reclaiming. */
#define RESOURCE_LEVEL_LOW	2	/**< Low. */
#define RESOURCE_LEVEL_CRITICAL	3	/**< Critically low. */

/** Priorities for LRM handlers. */
#define LRM_CACHE_PRIORITY	0
#define LRM_FS_PRIORITY		1
#define LRM_SLAB_PRIORITY	2
#define LRM_DEFAULT_PRIORITY	100

/** Type of a low resource handler. */
typedef struct lrm_handler {
	list_t header;			/**< Link to handler list. */

	uint32_t types;			/**< Resource types the handler can reclaim. */
	uint32_t priority;		/**< Priority of the handler (0 = highest). */

	/** Attempt to reclaim the resource.
	 * @param level		Level of the resource. This can be used to
	 *			determine how much to reclaim. If the handler
	 *			can reclaim multiple resource types, this
	 *			parameter will be the lowest out of the levels
	 *			of all the resources. */
	void (*func)(int level);
} lrm_handler_t;

extern void lrm_handler_register(lrm_handler_t *handler);
extern void lrm_handler_unregister(lrm_handler_t *handler);

extern int lrm_level(uint32_t types);
extern void lrm_reclaim(uint32_t type);

extern int kdbg_cmd_lrm(int argc, char **argv);

extern void lrm_init(void);

#endif /* __LRM_H */
