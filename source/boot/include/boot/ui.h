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
 * @brief		User interface functions.
 */

#ifndef __BOOT_UI_H
#define __BOOT_UI_H

#include <boot/config.h>
#include <boot/console.h>

struct ui_entry;
struct ui_window;

/** Return codes for input handling functions. */
typedef enum input_result {
	INPUT_HANDLED,			/**< No special action needed. */
	INPUT_RENDER,			/**< Re-render the window. */
	INPUT_CLOSE,			/**< Close the window. */
} input_result_t;

/** Structure defining a window action. */
typedef struct ui_action {
	const char *name;		/**< Name of action. */
	uint16_t key;			/**< Key to perform action. */

	/** Callback for the action.
	 * @param entry		Entry the action was performed on.
	 * @return		Input handling result. */
	input_result_t (*cb)(struct ui_entry *entry);
} ui_action_t;

/** Structure defining a window type. */
typedef struct ui_window_type {
	/** Render the window.
	 * @note		The draw region will be set to the content area.
	 * @param window	Window to render. */
	void (*render)(struct ui_window *window);

	/** Write the help text for the window.
	 * @note		The cursor will be positioned where to write.
	 * @note		This is called after each action is handled.
	 * @param window	Window to write for. */
	void (*help)(struct ui_window *window);

	/** Handle input on the window.
	 * @note		The draw region will be set to the content area.
	 * @param window	Window input was performed on.
	 * @param key		Key that was pressed.
	 * @return		Input handling result. */
	input_result_t (*input)(struct ui_window *window, uint16_t key);
} ui_window_type_t;

/** Window header structure. */
typedef struct ui_window {
	ui_window_type_t *type;		/**< Type of the window. */
	const char *title;		/**< Title of the window. */
} ui_window_t;

/** Structure defining a UI list entry type. */
typedef struct ui_entry_type {
	ui_action_t *actions;		/**< Actions that can be performed on the entry. */
	size_t action_count;		/**< Number of actions in the array. */

	/** Render the entry.
	 * @note		The draw region will set to where to render.
	 * @param entry		Entry to render. */
	void (*render)(struct ui_entry *entry);
} ui_entry_type_t;

/** List entry header structure. */
typedef struct ui_entry {
	ui_entry_type_t *type;		/**< Type of the entry. */
} ui_entry_t;

/** Size of the content region. */
#define UI_CONTENT_WIDTH	((size_t)main_console->width - 2)
#define UI_CONTENT_HEIGHT	((size_t)main_console->height - 4)

extern void ui_action_print(ui_action_t *action);

extern void ui_window_init(ui_window_t *window, ui_window_type_t *type, const char *title);
extern void ui_window_display(ui_window_t *window, int timeout);

extern ui_window_t *ui_textview_create(const char *title, const char *text);

extern ui_window_t *ui_list_create(const char *title, bool exitable);
extern void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected);
extern void ui_list_insert_env(ui_window_t *window, environ_t *env, const char *name,
                               const char *label, bool selected);

extern void ui_entry_init(ui_entry_t *entry, ui_entry_type_t *type);

extern ui_entry_t *ui_link_create(ui_window_t *window);
extern ui_entry_t *ui_checkbox_create(const char *label, value_t *value);
extern ui_entry_t *ui_textbox_create(const char *label, value_t *value);
extern ui_entry_t *ui_chooser_create(const char *label, value_t *value);
extern void ui_chooser_insert(ui_entry_t *entry, const char *name, void *value, bool selected);

#endif /* __BOOT_UI_H */
