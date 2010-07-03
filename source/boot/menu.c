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

#include <boot/loader.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/ui.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <kargs.h>
#include <time.h>

/** Structure containing a menu entry. */
typedef struct menu_entry {
	ui_entry_t header;		/**< UI entry header. */
	list_t link;			/**< Link to menu entries list. */
	char *name;			/**< Name of the entry. */
	environ_t *env;			/**< Environment for the entry. */
} menu_entry_t;

/** List of menu entries. */
static LIST_DECLARE(menu_entries);

/** Selected menu entry. */
static menu_entry_t *selected_menu_entry = NULL;

/** Commands that can be executed within a menu entry. */
static command_t menu_entry_commands[] = {
#if CONFIG_PLATFORM_PC
	{ "chainload",	config_cmd_chainload },
#endif
	{ "kiwi",	config_cmd_kiwi },
	{ "set",	config_cmd_set },
};

/** Add a new menu entry.
 * @param args		Arguments to the command.
 * @param env		Environment to operate on.
 * @return		Whether successful. */
bool config_cmd_entry(value_list_t *args, environ_t *env) {
	menu_entry_t *entry;

	assert(env == root_environ);

	if(args->count != 2 || args->values[0].type != VALUE_TYPE_STRING ||
	   args->values[1].type != VALUE_TYPE_COMMAND_LIST) {
		dprintf("config: entry: invalid arguments\n");
		return false;
	}

	entry = kmalloc(sizeof(menu_entry_t));
	list_init(&entry->link);
	entry->name = kstrdup(args->values[0].string);
	entry->env = environ_create();

	/* Execute the command list. */
	if(!command_list_exec(args->values[1].cmds, menu_entry_commands,
	                      ARRAYSZ(menu_entry_commands), entry->env)) {
		//environ_destroy(entry->env);
		kfree(entry->name);
		kfree(entry);
		return false;
	}

	list_append(&menu_entries, &entry->link);
	return true;
}

/** Find the default menu entry.
 * @return		Default entry. */
static menu_entry_t *menu_find_default(void) {
	menu_entry_t *entry;
	value_t *value;
	int i = 0;

	if((value = environ_lookup(root_environ, "default"))) {
		LIST_FOREACH(&menu_entries, iter) {
			entry = list_entry(iter, menu_entry_t, link);
			if(value->type == VALUE_TYPE_INTEGER) {
				if(i == value->integer) {
					return entry;
				}
			} else if(value->type == VALUE_TYPE_STRING) {
				if(strcmp(entry->name, value->string) == 0) {
					return entry;
				}
			}

			i++;
		}
	}

	/* No default entry found, return the first list entry. */
	return list_entry(menu_entries.next, menu_entry_t, link);
}

/** Check if the menu can be displayed.
 * @return		Whether the menu can be displayed. */
static bool menu_can_display(void) {
	value_t *value;

	if(!main_console) {
		return false;
	} else if((value = environ_lookup(root_environ, "hidden")) && value->integer == 1) {
		/* Menu hidden, wait half a second for Esc to be pressed. */
		spin(500000);
		while(main_console->check_key()) {
			if(main_console->get_key() == '\e') {
				return true;
			}
		}

		return false;
	} else {
		return true;
	}
}

/** Select a menu entry.
 * @param _entry	Entry that was selected.
 * @return		Always returns INPUT_CLOSE. */
static input_result_t menu_entry_select(ui_entry_t *_entry) {
	menu_entry_t *entry = (menu_entry_t *)_entry;
	selected_menu_entry = entry;
	return INPUT_CLOSE;
}

/** Configure a menu entry.
 * @param _entry	Entry that was selected.
 * @return		Always returns INPUT_RENDER. */
static input_result_t menu_entry_configure(ui_entry_t *_entry) {
	menu_entry_t *entry = (menu_entry_t *)_entry;
	loader_type_t *type = loader_type_get(entry->env);

	type->configure(entry->env);
	return INPUT_RENDER;
}

/** Actions for a menu entry. */
static ui_action_t menu_entry_actions[] = {
	{ "Boot", '\n', menu_entry_select },
};

/** Actions for a configurable menu entry. */
static ui_action_t configurable_menu_entry_actions[] = {
	{ "Boot", '\n', menu_entry_select },
	{ "Configure", CONSOLE_KEY_F1, menu_entry_configure },
};

/** Render a menu entry.
 * @param _entry	Entry to render. */
static void menu_entry_render(ui_entry_t *_entry) {
	menu_entry_t *entry = (menu_entry_t *)_entry;
	kprintf("%s", entry->name);
}

/** Menu entry UI entry type. */
static ui_entry_type_t menu_entry_type = {
	.actions = menu_entry_actions,
	.action_count = ARRAYSZ(menu_entry_actions),
	.render = menu_entry_render,
};

/** Configurable menu entry UI entry type. */
static ui_entry_type_t configurable_menu_entry_type = {
	.actions = configurable_menu_entry_actions,
	.action_count = ARRAYSZ(configurable_menu_entry_actions),
	.render = menu_entry_render,
};

/** Display the menu interface.
 * @return		Environment for the entry to boot. */
environ_t *menu_display(void) {
	menu_entry_t *entry;
	ui_window_t *window;
	int timeout = 0;
	value_t *value;

	if(list_empty(&menu_entries)) {
		boot_error("No entries defined in configuration");
	}

	/* Find the default entry. */
	selected_menu_entry = menu_find_default();

	if(menu_can_display()) {
		/* Construct the menu. */
		window = ui_list_create("Boot Menu", false);
		LIST_FOREACH(&menu_entries, iter) {
			entry = list_entry(iter, menu_entry_t, link);

			/* If the entry's loader type has a configure function,
			 * use the configurable entry type. */
			if(loader_type_get(entry->env)->configure) {
				ui_entry_init(&entry->header, &configurable_menu_entry_type);
			} else {
				ui_entry_init(&entry->header, &menu_entry_type);
			}
			ui_list_insert(window, &entry->header, entry == selected_menu_entry);
		}

		/* Display it. The selected entry pointer will be updated. */
		if((value = environ_lookup(root_environ, "timeout")) && value->type == VALUE_TYPE_INTEGER) {
			timeout = value->integer;
		}
		ui_window_display(window, timeout);
	}

	dprintf("loader: booting menu entry '%s'\n", selected_menu_entry->name);
	return selected_menu_entry->env;
}
