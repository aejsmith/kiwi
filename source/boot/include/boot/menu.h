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
 * @brief		Bootloader menu interface.
 */

#ifndef __BOOT_MENU_H
#define __BOOT_MENU_H

#include <lib/list.h>

/** Structure representing a menu. */
typedef struct menu {
	const char *title;		/**< Title of the menu. */
	list_t items;			/**< List of items in the menu. */
	size_t count;			/**< Number of items. */
} menu_t;

/** Structure representing a menu item. */
typedef struct menu_item {
	list_t header;			/**< Link to item list. */

	const char *name;		/**< Name of the item. */

	/** Type of the item. */
	enum {
		MENU_ITEM_SUBMENU,	/**< Sub-menu. */
		MENU_ITEM_CHECKBOX,	/**< Checkbox. */
		MENU_ITEM_EXIT,		/**< Exit button. */
		MENU_ITEM_CHOICE,	/**< Multiple choice. */
	} type;

	void *value;			/**< Pointer for the item's value (choice/exit). */
	menu_t *menu;			/**< Menu implementing the choice/submenu. */
	bool *checked;			/**< Where to store checked value. */
} menu_item_t;

extern void arch_add_menu_options(menu_t *menu, menu_t *options);
extern void platform_add_menu_options(menu_t *menu, menu_t *options);

extern void menu_item_add_choice(menu_item_t *item, const char *name, void *value, bool selected);

extern menu_t *menu_add_submenu(menu_t *menu, const char *name);
extern void menu_add_checkbox(menu_t *menu, const char *name, bool *checkedp);
extern void menu_add_exit(menu_t *menu, const char *name, void *value);
extern menu_item_t *menu_add_choice(menu_t *menu, const char *name);

extern void menu_display(void);

#endif /* __BOOT_MENU_H */
