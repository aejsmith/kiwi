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
 * @brief		User interface functions.
 */

#include <boot/memory.h>
#include <boot/ui.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>

/** Details of a line in a text view. */
typedef struct ui_textview_line {
	const char *ptr;		/**< Pointer to the line. */
	size_t len;			/**< Length of the line. */
} ui_textview_line_t;

/** Structure containing a text view window. */
typedef struct ui_textview {
	ui_window_t header;		/**< Window header. */

	/** Array containing details of each line. */
	struct {
		const char *ptr;	/**< Pointer to the line. */
		size_t len;		/**< Length of the line. */
	} *lines;

	size_t count;			/**< Number of lines. */
	size_t offset;			/**< Current offset. */
} ui_textview_t;

/** Structure containing a list window. */
typedef struct ui_list {
	ui_window_t header;		/**< Window header. */
	list_t entries;			/**< List of entries. */
	size_t count;			/**< Number of entries. */
	size_t selected;		/**< Index of selected entry. */
} ui_list_t;

/** Set the region to the title region.
 * @param clear		Whether to clear the region. */
static inline void set_title_region(bool clear) {
	draw_region_t r = { 0, 0, main_console->width, 1, false };
	main_console->set_region(&r);
	if(clear) {
		main_console->clear(0, 0, r.width, r.height);
	}
}

/** Set the region to the help region.
 * @param clear		Whether to clear the region. */
static inline void set_help_region(bool clear) {
	draw_region_t r = { 0, main_console->height - 1, main_console->width, 1, false };
	main_console->set_region(&r);
	if(clear) {
		main_console->clear(0, 0, r.width, r.height);
	}
}

/** Set the region to the content region.
 * @param clear		Whether to clear the region. */
static inline void set_content_region(bool clear) {
	draw_region_t r = { 1, 2, main_console->width - 2, UI_CONTENT_HEIGHT, false };
	main_console->set_region(&r);
	if(clear) {
		main_console->clear(0, 0, r.width, r.height);
	}
}

/** Print an action (for help text).
 * @param action	Action to print. */
void ui_action_print(ui_action_t *action) {
	switch(action->key) {
	case CONSOLE_KEY_UP:
		kprintf("Up");
		break;
	case CONSOLE_KEY_DOWN:
		kprintf("Down");
		break;
	case CONSOLE_KEY_LEFT:
		kprintf("Left");
		break;
	case CONSOLE_KEY_RIGHT:
		kprintf("Right");
		break;
	case CONSOLE_KEY_F1:
		kprintf("F1");
		break;
	case CONSOLE_KEY_F2:
		kprintf("F2");
		break;
	case '\n':
		kprintf("Enter");
		break;
	case '\e':
		kprintf("Esc");
		break;
	default:
		kprintf("%c", action->key & 0xFF);
		break;
	}

	kprintf(" = %s  ", action->name);
}

/** Initialise a window structure.
 * @param window	Window to initialise.
 * @param type		Type of window.
 * @param title		Title of the window. */
void ui_window_init(ui_window_t *window, ui_window_type_t *type, const char *title) {
	window->type = type;
	window->title = title;
}

/** Render help text for a window.
 * @param window	Window to render help text for.
 * @param exitable	Whether the window is exitable. */
static void ui_window_render_help(ui_window_t *window, bool exitable) {
	/* Set the region and clear it. */
	set_help_region(true);

	/* Write the new text. */
	window->type->help(window);

	/* If exitable, print that too. */
	if(exitable) {
		kprintf("Esc = Back");
	}

	/* Highlight it. */
	main_console->highlight(0, 0, main_console->width, 1);
}

/** Render the contents of a window.
 * @note		Draw region will be content region after returning.
 * @param window	Window to render.
 * @param exitable	Whether the window is exitable. */
static void ui_window_render(ui_window_t *window, bool exitable) {
	main_console->reset();

	set_title_region(true);
	kprintf("%s", window->title);
	main_console->highlight(0, 0, main_console->width, 1);

	ui_window_render_help(window, exitable);

	set_content_region(true);
	window->type->render(window);
}

/** Display a window.
 * @param window	Window to display.
 * @param exitable	Whether the window can be exited. */
void ui_window_display(ui_window_t *window, bool exitable) {
	bool exited = false;
	uint16_t key;

	while(!exited) {
		ui_window_render(window, exitable);
		while(true) {
			key = main_console->get_key();

			/* Exit if the key is Escape and we're exitable. */
			if(key == '\e' && exitable) {
				exited = true;
				break;
			}

			/* If the call returns true, the window must be
			 * re-rendered, so break out of this loop. */
			set_content_region(false);
			if(window->type->input(window, key)) {
				break;
			}

			/* Need to re-render help text each key press, for
			 * example if the action moved to a different list
			 * entry with different actions. */
			ui_window_render_help(window, exitable);
		}
	}

	main_console->reset();
}

/** Print a line from a text view.
 * @param view		View to render.
 * @param line		Index of line to print. */
static void ui_textview_render_line(ui_textview_t *view, size_t line) {
	size_t i;

	for(i = 0; i < view->lines[line].len; i++) {
		main_console->putch(view->lines[line].ptr[i]);
	}

	if(view->lines[line].len < UI_CONTENT_WIDTH) {
		main_console->putch('\n');
	}
}

/** Render a textview window.
 * @param window	Window to render. */
static void ui_textview_render(ui_window_t *window) {
	ui_textview_t *view = (ui_textview_t *)window;
	size_t i;

	for(i = view->offset; i < MIN(view->offset + UI_CONTENT_HEIGHT, view->count); i++) {
		ui_textview_render_line(view, i);
	}
}

/** Write the help text for a textview window.
 * @param window	Window to write for. */
static void ui_textview_help(ui_window_t *window) {
	ui_textview_t *view = (ui_textview_t *)window;

	if(view->offset) {
		kprintf("Up = Scroll Up  ");
	}
	if((view->count - view->offset) > UI_CONTENT_HEIGHT) {
		kprintf("Down = Scroll Down  ");
	}
}

/** Handle input on the window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Whether to re-render the window. */
static bool ui_textview_input(ui_window_t *window, uint16_t key) {
	ui_textview_t *view = (ui_textview_t *)window;

	switch(key) {
	case CONSOLE_KEY_UP:
		if(view->offset) {
			main_console->scroll_up();
			ui_textview_render_line(view, --view->offset);
		}
		break;
	case CONSOLE_KEY_DOWN:
		if((view->count - view->offset) > UI_CONTENT_HEIGHT) {
			main_console->scroll_down();
			main_console->move_cursor(0, -1);
			ui_textview_render_line(view, view->offset++ + UI_CONTENT_HEIGHT);
		}
		break;
	}

	return false;
}

/** Text view window type. */
static ui_window_type_t textview_window_type = {
	.render = ui_textview_render,
	.help = ui_textview_help,
	.input = ui_textview_input,
};

/** Add a line to a text view.
 * @param view		View to add to.
 * @param line		Line to add.
 * @param len		Length of line. */
static void ui_textview_add_line(ui_textview_t *view, const char *line, size_t len) {
	/* If the line is larger than the content width, split it. */
	if(len > UI_CONTENT_WIDTH) {
		ui_textview_add_line(view, line, UI_CONTENT_WIDTH);
		ui_textview_add_line(view, line + UI_CONTENT_WIDTH, len - UI_CONTENT_WIDTH);
	} else {
		view->lines = krealloc(view->lines, sizeof(*view->lines) * (view->count + 1));
		view->lines[view->count].ptr = line;
		view->lines[view->count++].len = len;
	}
}

/** Create a text view window.
 * @param title		Title for the window.
 * @param text		Buffer containing text to display.
 * @return		Pointer to created window. */
ui_window_t *ui_textview_create(const char *title, const char *text) {
	ui_textview_t *view = kmalloc(sizeof(ui_textview_t));
	const char *tok;
	size_t len;

	ui_window_init(&view->header, &textview_window_type, title);
	view->lines = NULL;
	view->count = 0;

	/* Store details of all the lines in the buffer. */
	while((tok = strchr(text, '\n'))) {
		ui_textview_add_line(view, text, tok - text);
		text = tok + 1;
	}

	/* If there is still data at the end (no newline before end), add it. */
	if((len = strlen(text)) > 0) {
		ui_textview_add_line(view, text, len);
	}
	
	return &view->header;
}

/** Create a list window.
 * @param title		Title for the window.
 * @return		Pointer to created window. */
ui_window_t *ui_list_create(const char *title) {
#if 0
	ui_list_t *list = kmalloc(sizeof(ui_list_t));

	ui_window_init(&list->header, &list_window_type, title);
	list_init(&list->entries);
	list->count = 0;
	list->selected = 0;
	return &list->header;
#endif
	return NULL;
}

/** Insert an entry into a list window.
 * @param window	Window to insert into.
 * @param entry		Entry to insert.
 * @param selected	Whether the entry should be selected. */
void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected) {
	ui_list_t *list = (ui_list_t *)window;

	list_append(&list->entries, &entry->header);
	if(selected) {
		list->selected = list->count++;
	} else {
		list->count++;
	}
}

/** Initialise a list entry structure.
 * @param entry		Entry structure to initialise.
 * @param type		Type of the entry. */
void ui_entry_init(ui_entry_t *entry, ui_entry_type_t *type) {
	list_init(&entry->header);
	entry->type = type;
}

/** Create a label entry.
 * @param text		Text that the label will display.
 * @return		Pointer to created entry. */
ui_entry_t *ui_label_create(const char *text) {
	return NULL;
}

/** Create a checkbox entry.
 * @param label		Label for the checkbox.
 * @param value		Value to store state in (should be VALUE_TYPE_INTEGER).
 * @return		Pointer to created entry. */
ui_entry_t *ui_checkbox_create(const char *label, value_t *value) {
	return NULL;
}

/** Create a textbox entry.
 * @param label		Label for the textbox.
 * @param value		Value to store state in (should be VALUE_TYPE_STRING).
 * @return		Pointer to created entry. */
ui_entry_t *ui_textbox_create(const char *label, value_t *value) {
	return NULL;
}

/** Create a choice entry.
 * @param label		Label for the entry.
 * @param value		Value to store state in (should be VALUE_TYPE_POINTER).
 * @return		Pointer to created entry. */
ui_entry_t *ui_choice_create(const char *name, value_t *value) {
	return NULL;
}

/** Insert a choice into a choice entry.
 * @param entry		Entry to insert into.
 * @param name		Name of the choice.
 * @param value		Value of the choice. */
void ui_choice_insert(ui_entry_t *entry, const char *name, void *value) {

}
