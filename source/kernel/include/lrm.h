/*
 * Copyright (C) 2010 Alex Smith
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
extern void lrm_reclaim(uint32_t type, uint64_t required);

extern int kdbg_cmd_lrm(int argc, char **argv);

extern void lrm_init(void);

#endif /* __LRM_H */
