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

#include <boot/console.h>
#include <boot/cpu.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/vfs.h>

#include <assert.h>
#include <kargs.h>

static void *menu_display_real(menu_t *menu, size_t index);

/** Print a menu item's title.
 * @param item		Item to print. */
static void menu_item_print(menu_item_t *item) {
	menu_item_t *child;

	switch(item->type) {
	case MENU_ITEM_SUBMENU:
		kprintf(" %s...\n", item->name);
		break;
	case MENU_ITEM_CHECKBOX:
		kprintf(" [%c] %s\n", (*item->checked) ? 'x' : ' ', item->name);
		break;
	case MENU_ITEM_EXIT:
		kprintf(" %s\n", item->name);
		break;
	case MENU_ITEM_CHOICE:
		LIST_FOREACH(&item->menu->items, iter) {
			child = list_entry(iter, menu_item_t, header);
			if(child->value == item->value) {
				kprintf(" %s (Current: %s)\n", item->name, child->name);
				break;
			}
		}
		break;
	}
}

/** Select a menu item.
 * @param item		Item to select.
 * @param retp		Where to store return value.
 * @return		Whether to exit the menu. */
static bool menu_item_select(menu_item_t *item, void **retp) {
	menu_item_t *child;
	size_t i = 0;

	switch(item->type) {
	case MENU_ITEM_SUBMENU:
		menu_display_real(item->menu, 0);
		return false;
	case MENU_ITEM_CHECKBOX:
		*item->checked = !*item->checked;
		return false;
	case MENU_ITEM_EXIT:
		*retp = item->value;
		return true;
	case MENU_ITEM_CHOICE:
		/* Find the index of the selected item. */
		LIST_FOREACH(&item->menu->items, iter) {
			child = list_entry(iter, menu_item_t, header);
			if(child->value == item->value) {
				break;
			} else {
				i++;
			}
		}
		item->value = menu_display_real(item->menu, i);
		return false;
	default:
		return false;
	}
}

/** Display a menu.
 * @param menu		Menu to display.
 * @param index		Index of item to be selected initially.
 * @return		Value requested to be returned. */
static void *menu_display_real(menu_t *menu, size_t index) {
	size_t total, offset, i;
	menu_item_t *item;
	bool exit = false;
	void *ret = NULL;
	uint16_t ch;

	/* Determine the number of items that can be fit on at a time. */
	total = main_console.height - 4;

	/* Set the initial offset based on the selected index. */
	offset = 0;
	if(index >= total) {
		offset = ((index - total) + 1);
		index -= offset;
	}

	/* Loop until an item requests to exit the menu. */
	while(!exit) {
		main_console.clear();

		/* Display a header. */
		kprintf(menu->title);
		main_console.highlight(0, 0, main_console.width, 1);

		/* Add instructions. */
		main_console.move_cursor(0, main_console.height - 1);
		kprintf("Enter/Space = Select   Up = Move up   Down = Move down");
		main_console.highlight(0, main_console.height - 1, main_console.width, 1);

		/* Display each item. */
		main_console.move_cursor(0, 2);
		i = 0;
		LIST_FOREACH(&menu->items, iter) {
			if(i < offset || ((menu->count - offset) >= total && i >= (offset + total))) {
				i++;
				continue;
			}
			i++;
			item = list_entry(iter, menu_item_t, header);
			menu_item_print(item);
		}

		/* Loop getting keypresses. */
		while(true) {
			/* highlight current entry, get a keypress and then
			 * unhighlight before modifying anything. */
			main_console.highlight(1, index + 2, main_console.width - 2, 1);
			ch = main_console.getch();
			main_console.highlight(1, index + 2, main_console.width - 2, 1);

			/* Handle the character. */
			if(ch == CONSOLE_KEY_UP) {
				if(index) {
					index--;
				} else if(offset) {
					/* Have to redraw after changing offset. */
					offset--;
					break;
				}
			} else if(ch == CONSOLE_KEY_DOWN) {
				if((index + offset + 1) < menu->count) {
					if((index + 1) < total) {
						index++;
					} else {
						/* Have to redraw after changing offset. */
						offset++;
						break;
					}
				}
			} else if(ch == '\r' || ch == ' ') {
				i = 0;
				LIST_FOREACH(&menu->items, iter) {
					if(i++ == (index + offset)) {
						item = list_entry(iter, menu_item_t, header);
						exit = menu_item_select(item, &ret);
						break;
					}
				}
				break;
			}
		}
	}

	/* Clear the console. */
	main_console.clear();

	return ret;
}

/** Create a new menu.
 * @param title		Title for the menu.
 * @return		Pointer to the menu structure. */
static menu_t *menu_create(const char *title) {
	menu_t *menu = kmalloc(sizeof(menu_t));

	list_init(&menu->items);
	menu->title = title;
	menu->count = 0;
	return menu;
}

/** Add a choice to a choice menu item.
 * @param item		Item to add to.
 * @param name		Name of the choice.
 * @param value		Value for the choice.
 * @param selected	Whether the item should currently be selected. */
void menu_item_add_choice(menu_item_t *item, const char *name, void *value, bool selected) {
	assert(item->type == MENU_ITEM_CHOICE);

	if(list_empty(&item->menu->items) || selected) {
		item->value = value;
	}
	menu_add_exit(item->menu, name, value);
}

/** Add a submenu to a menu.
 * @param menu		Menu to add to.
 * @param name		Name to give submenu.
 * @return		Pointer to menu structure. */
menu_t *menu_add_submenu(menu_t *menu, const char *name) {
	menu_item_t *item = kmalloc(sizeof(menu_item_t));

	list_init(&item->header);
	item->name = name;
	item->type = MENU_ITEM_SUBMENU;
	item->menu = menu_create(name);

	list_append(&menu->items, &item->header);
	menu->count++;
	return item->menu;
}

/** Add a checkbox to a menu.
 * @param menu		Menu to add to.
 * @param name		Name to give the menu entry.
 * @param checkedp	Where to store the value of the checkbox. */
void menu_add_checkbox(menu_t *menu, const char *name, bool *checkedp) {
	menu_item_t *item = kmalloc(sizeof(menu_item_t));

	list_init(&item->header);
	item->name = name;
	item->type = MENU_ITEM_CHECKBOX;
	item->checked = checkedp;

	list_append(&menu->items, &item->header);
	menu->count++;
}

/** Add an exit button to a menu.
 * @param menu		Menu to add to.
 * @param name		Name to give the menu entry.
 * @param value		Value to make the menu return with. */
void menu_add_exit(menu_t *menu, const char *name, void *value) {
	menu_item_t *item = kmalloc(sizeof(menu_item_t));

	list_init(&item->header);
	item->name = name;
	item->type = MENU_ITEM_EXIT;
	item->value = value;

	list_append(&menu->items, &item->header);
	menu->count++;
}

/** Add a choice to a menu.
 * @param menu		Menu to add to.
 * @param name		Name to give the menu entry.
 * @return		Pointer to menu item that can have choices added to. */
menu_item_t *menu_add_choice(menu_t *menu, const char *name) {
	menu_item_t *item = kmalloc(sizeof(menu_item_t));

	list_init(&item->header);
	item->name = name;
	item->type = MENU_ITEM_CHOICE;
	item->menu = menu_create(name);
	item->value = NULL;

	list_append(&menu->items, &item->header);
	menu->count++;
	return item;
}

/** Display the menu interface. */
void menu_display(void) {
	menu_t *menu, *options;
	vfs_filesystem_t *fs;
	menu_item_t *volume;

	/* Give the user a short time to hold down shift. */
	spin(300000);
	if(!main_console.shift_held()) {
		return;
	}

	/* Build the menu interface. */
	menu = menu_create("Boot Options");
	menu_add_exit(menu, "Continue booting", NULL);

	/* Add a list to choose the boot filesystem. */
	volume = menu_add_choice(menu, "Boot Volume");
	LIST_FOREACH(&filesystem_list, iter) {
		fs = list_entry(iter, vfs_filesystem_t, header);
		menu_item_add_choice(volume, fs->label, fs, fs == boot_filesystem);
	}

	/* Add a menu to set kernel options. */
	options = menu_add_submenu(menu, "Kernel Options");
	menu_add_exit(options, "Return to main menu", NULL);
	menu_add_checkbox(options, "Disable SMP", &kernel_args->smp_disabled);
	menu_add_checkbox(options, "Disable boot splash", &kernel_args->splash_disabled);

	/* Add architecture/platform options. */
	arch_add_menu_options(menu, options);
	platform_add_menu_options(menu, options);

	/* Display the menu. */
	menu_display_real(menu, 0);

	/* Pull the boot filesystem option out. */
	boot_filesystem = (vfs_filesystem_t *)volume->value;
}
