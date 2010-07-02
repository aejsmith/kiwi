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
 *
 * @todo		Allow moving around the buffer in the textbox editor,
 *			rather than only allowing edits at the end.
 */

#include <boot/memory.h>
#include <boot/ui.h>

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>

struct ui_choice;
struct ui_textbox;

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
	ui_entry_t **entries;		/**< Array of entries. */
	size_t count;			/**< Number of entries. */
	size_t offset;			/**< Offset of first entry displayed. */
	size_t selected;		/**< Index of selected entry. */
} ui_list_t;

/** Structure containing a link. */
typedef struct ui_link {
	ui_entry_t header;		/**< Entry header. */
	ui_window_t *window;		/**< Window that this links to. */
} ui_link_t;

/** Structure containing a checkbox. */
typedef struct ui_checkbox {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the checkbox. */
	value_t *value;			/**< Value of the checkbox. */
} ui_checkbox_t;

/** Structure containing a textbox. */
typedef struct ui_textbox {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the textbox. */
	value_t *value;			/**< Value of the textbox. */
	ui_window_t *editor;		/**< Editor window for the textbox. */
} ui_textbox_t;

/** Structure containing a chooser. */
typedef struct ui_chooser {
	ui_entry_t header;		/**< Entry header. */
	const char *label;		/**< Label for the choice. */
	struct ui_choice *selected;	/**< Selected item. */
	value_t *value;			/**< Value to update. */
	ui_window_t *list;		/**< List implementing the chooser. */
} ui_chooser_t;

/** Structure containing an choice. */
typedef struct ui_choice {
	ui_entry_t header;		/**< Entry header. */
	ui_chooser_t *chooser;		/**< Chooser that the entry is for. */
	const char *name;		/**< Name of the choice. */
	void *value;			/**< Value of this entry. */
} ui_choice_t;

/** State for the textbox editor. */
static char textbox_edit_buf[1024];
static size_t textbox_edit_len = 0;
static bool textbox_edit_update = false;

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
 * @param window	Window to render help text for. */
static void ui_window_render_help(ui_window_t *window) {
	set_help_region(true);
	window->type->help(window);
	main_console->highlight(0, 0, main_console->width, 1);
}

/** Render the contents of a window.
 * @note		Draw region will be content region after returning.
 * @param window	Window to render. */
static void ui_window_render(ui_window_t *window) {
	main_console->reset();

	set_title_region(true);
	kprintf("%s", window->title);
	main_console->highlight(0, 0, main_console->width, 1);

	ui_window_render_help(window);

	set_content_region(true);
	window->type->render(window);
}

/** Display a window.
 * @param window	Window to display. */
void ui_window_display(ui_window_t *window) {
	bool exited = false;
	input_result_t ret;
	uint16_t key;

	while(!exited) {
		ui_window_render(window);
		while(true) {
			key = main_console->get_key();
			set_content_region(false);
			if((ret = window->type->input(window, key)) != INPUT_HANDLED) {
				if(ret == INPUT_CLOSE) {
					exited = true;
				}
				break;
			}

			/* Need to re-render help text each key press, for
			 * example if the action moved to a different list
			 * entry with different actions. */
			ui_window_render_help(window);
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
	kprintf("Esc = Back");
}

/** Handle input on the window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_textview_input(ui_window_t *window, uint16_t key) {
	ui_textview_t *view = (ui_textview_t *)window;
	input_result_t ret = INPUT_HANDLED;

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
	case '\e':
		ret = INPUT_CLOSE;
		break;
	}

	return ret;
}

/** Text view window type. */
static ui_window_type_t ui_textview_window_type = {
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

	ui_window_init(&view->header, &ui_textview_window_type, title);
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

/** Render an entry from a list.
 * @note		Current draw region should be the content region.
 * @param entry		Entry to render.
 * @param pos		Position to render at.
 * @param selected	Whether to highlight. */
static void ui_list_render_entry(ui_entry_t *entry, size_t pos, bool selected) {
	draw_region_t region, content;

	/* Work out where to put the entry. */
	main_console->get_region(&content);
	region.x = content.x;
	region.y = content.y + pos;
	region.width = content.width;
	region.height = 1;
	region.scrollable = false;
	main_console->set_region(&region);

	/* Render the entry. */
	entry->type->render(entry);

	/* Highlight if necessary. */
	if(selected) {
		main_console->highlight(0, 0, region.width, 1);
	}

	/* Restore content region. */
	main_console->set_region(&content);
}

/** Render a list window.
 * @param window	Window to render. */
static void ui_list_render(ui_window_t *window) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i;

	/* Render each entry. */
	for(i = list->offset; i < MIN(list->offset + UI_CONTENT_HEIGHT, list->count - list->offset); i++) {
		ui_list_render_entry(list->entries[i], i - list->offset, list->selected == i);
	}
}

/** Write the help text for a list window.
 * @param window	Window to write for. */
static void ui_list_help(ui_window_t *window) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i;

	/* Print help for each of the selected entry's actions. */
	for(i = 0; i < list->entries[list->selected]->type->action_count; i++) {
		ui_action_print(&list->entries[list->selected]->type->actions[i]);
	}

	/* Print navigation instructions. */
	if(list->selected > 0) {
		kprintf("Up = Scroll Up  ");
	}
	if(list->selected < (list->count - 1)) {
		kprintf("Down = Scroll Down  ");
	}
	kprintf("Esc = Back");
}

/** Handle input on the window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_list_input(ui_window_t *window, uint16_t key) {
	ui_list_t *list = (ui_list_t *)window;
	input_result_t ret = INPUT_HANDLED;
	size_t i;

	switch(key) {
	case CONSOLE_KEY_UP:
		if(!list->selected) {
			break;
		}

		/* Un-highlight current entry. */
		main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);

		/* If selected becomes less than the offset, must scroll up. */
		if(--list->selected < list->offset) {
			list->offset--;
			main_console->scroll_up();
			ui_list_render_entry(list->entries[list->selected], 0, true);
		} else {
			/* Highlight new entry. */
			main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);
		}
		break;
	case CONSOLE_KEY_DOWN:
		if(list->selected >= (list->count - 1)) {
			break;
		}

		/* Un-highlight current entry. */
		main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);

		/* If selected is now off screen, must scroll down. */
		if(++list->selected >= list->offset + UI_CONTENT_HEIGHT) {
			list->offset++;
			main_console->scroll_down();
			ui_list_render_entry(list->entries[list->selected], UI_CONTENT_HEIGHT - 1, true);
		} else {
			/* Highlight new entry. */
			main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);
		}
		break;
	case '\e':
		ret = INPUT_CLOSE;
		break;
	default:
		/* Handle custom actions. */
		for(i = 0; i < list->entries[list->selected]->type->action_count; i++) {
			if(key != list->entries[list->selected]->type->actions[i].key) {
				continue;
			}

			ret = list->entries[list->selected]->type->actions[i].cb(list->entries[list->selected]);
			if(ret == INPUT_HANDLED) {
				/* Need to re-render the entry. */
				main_console->highlight(0, list->selected - list->offset, UI_CONTENT_WIDTH, 1);
				ui_list_render_entry(list->entries[list->selected],
				                     list->selected - list->offset,
				                     true);
			}
			break;
		}
		break;
	}
	return ret;
}

/** List window type. */
static ui_window_type_t ui_list_window_type = {
	.render = ui_list_render,
	.help = ui_list_help,
	.input = ui_list_input,
};

/** Create a list window.
 * @param title		Title for the window.
 * @return		Pointer to created window. */
ui_window_t *ui_list_create(const char *title) {
	ui_list_t *list = kmalloc(sizeof(ui_list_t));

	ui_window_init(&list->header, &ui_list_window_type, title);
	list->entries = NULL;
	list->count = 0;
	list->offset = 0;
	list->selected = 0;
	return &list->header;
}

/** Insert an entry into a list window.
 * @param window	Window to insert into.
 * @param entry		Entry to insert.
 * @param selected	Whether the entry should be selected. */
void ui_list_insert(ui_window_t *window, ui_entry_t *entry, bool selected) {
	ui_list_t *list = (ui_list_t *)window;
	size_t i = list->count++;

	list->entries = krealloc(list->entries, sizeof(ui_entry_t *) * list->count);
	list->entries[i] = entry;
	if(selected) {
		list->selected = i;
		if(i >= UI_CONTENT_HEIGHT) {
			list->offset = i - UI_CONTENT_HEIGHT + 1;
		}
	}
}

/** Initialise a list entry structure.
 * @param entry		Entry structure to initialise.
 * @param type		Type of the entry. */
void ui_entry_init(ui_entry_t *entry, ui_entry_type_t *type) {
	entry->type = type;
}

/** Select a link.
 * @param entry		Entry to select.
 * @return		Input handling result. */
static input_result_t ui_link_select(ui_entry_t *entry) {
	ui_link_t *link = (ui_link_t *)entry;
	ui_window_display(link->window);
	return INPUT_RENDER;
}

/** Actions for a link. */
static ui_action_t ui_link_actions[] = {
	{ "Select", '\n', ui_link_select },
};

/** Render a link.
 * @param entry		Entry to render. */
static void ui_link_render(ui_entry_t *entry) {
	ui_link_t *link = (ui_link_t *)entry;

	kprintf("%s", link->window->title);
	main_console->move_cursor(-2, 0);
	kprintf("->");
}

/** Link entry type. */
static ui_entry_type_t ui_link_entry_type = {
	.actions = ui_link_actions,
	.action_count = ARRAYSZ(ui_link_actions),
	.render = ui_link_render,
};

/** Create an entry which opens another window.
 * @param window	Window that the entry should open.
 * @return		Pointer to entry. */
ui_entry_t *ui_link_create(ui_window_t *window) {
	ui_link_t *link = kmalloc(sizeof(ui_link_t));

	ui_entry_init(&link->header, &ui_link_entry_type);
	link->window = window;
	return &link->header;
}

/** Toggle the value of a checkbox.
 * @param entry		Entry to toggle.
 * @return		Input handling result. */
static input_result_t ui_checkbox_toggle(ui_entry_t *entry) {
	ui_checkbox_t *box = (ui_checkbox_t *)entry;
	box->value->integer = !box->value->integer;
	return INPUT_HANDLED;
}

/** Actions for a check box. */
static ui_action_t ui_checkbox_actions[] = {
	{ "Toggle", '\n', ui_checkbox_toggle },
};

/** Render a check box.
 * @param entry		Entry to render. */
static void ui_checkbox_render(ui_entry_t *entry) {
	ui_checkbox_t *box = (ui_checkbox_t *)entry;

	kprintf("%s", box->label);
	main_console->move_cursor(-3, 0);
	kprintf("[%c]", (box->value->integer) ? 'x' : ' ');
}

/** Check box entry type. */
static ui_entry_type_t ui_checkbox_entry_type = {
	.actions = ui_checkbox_actions,
	.action_count = ARRAYSZ(ui_checkbox_actions),
	.render = ui_checkbox_render,
};

/** Create a checkbox entry.
 * @param label		Label for the checkbox.
 * @param value		Value to store state in (should be VALUE_TYPE_INTEGER).
 * @return		Pointer to created entry. */
ui_entry_t *ui_checkbox_create(const char *label, value_t *value) {
	ui_checkbox_t *box = kmalloc(sizeof(ui_checkbox_t));

	assert(value->type == VALUE_TYPE_INTEGER);

	ui_entry_init(&box->header, &ui_checkbox_entry_type);
	box->label = label;
	box->value = value;
	return &box->header;
}

/** Render a text box edit window.
 * @param window	Window to render. */
static void ui_textbox_editor_render(ui_window_t *window) {
	size_t i;

	for(i = 0; i < textbox_edit_len; i++) {
		main_console->putch(textbox_edit_buf[i]);
	}
}

/** Write the help text for a text box edit window.
 * @param window	Window to write for. */
static void ui_textbox_editor_help(ui_window_t *window) {
	kprintf("Enter = Update  Esc = Cancel");
}

/** Handle input on a text box edit window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t ui_textbox_editor_input(ui_window_t *window, uint16_t key) {
	char ch;

	switch(key) {
	case '\n':
		textbox_edit_update = true;
	case '\e':
		return INPUT_CLOSE;
	default:
		/* Ignore non-printable keys. */
		if(!isprint(key) && key != '\b') {
			return INPUT_HANDLED;
		}

		ch = key & 0xFF;
		if(ch == '\b') {
			textbox_edit_len--;
		} else if(textbox_edit_len < ARRAYSZ(textbox_edit_buf)) {
			textbox_edit_buf[textbox_edit_len++] = ch;
		}

		/* FIXME: I'm lazy and cba to make this update the screen. */
		return INPUT_RENDER;
	}
}

/** Text box editor window type. */
static ui_window_type_t ui_textbox_editor_window_type = {
	.render = ui_textbox_editor_render,
	.help = ui_textbox_editor_help,
	.input = ui_textbox_editor_input,
};

/** Edit the value of a text box.
 * @param entry		Entry to edit.
 * @return		Input handling result. */
static input_result_t ui_textbox_edit(ui_entry_t *entry) {
	ui_textbox_t *box = (ui_textbox_t *)entry;
	size_t len;

	/* Copy the current string to the editing buffer. */
	len = strlen(box->value->string);
	memcpy(textbox_edit_buf, box->value->string, len);
	textbox_edit_len = len;
	textbox_edit_update = false;

	/* Display the editor. */
	ui_window_display(box->editor);

	/* Copy back the new string. */
	if(textbox_edit_update) {
		if(textbox_edit_len != len) {
			kfree(box->value->string);
			box->value->string = kmalloc(textbox_edit_len + 1);
		}
		memcpy(box->value->string, textbox_edit_buf, textbox_edit_len);
		box->value->string[textbox_edit_len] = 0;
	}
	return INPUT_RENDER;
}

/** Actions for a text box. */
static ui_action_t ui_textbox_actions[] = {
	{ "Edit", '\n', ui_textbox_edit },
};

/** Render a text box.
 * @param entry		Entry to render. */
static void ui_textbox_render(ui_entry_t *entry) {
	ui_textbox_t *box = (ui_textbox_t *)entry;
	size_t len, avail, i;

	kprintf("%s", box->label);

	/* Work out the length available to put the string value in. */
	avail = UI_CONTENT_WIDTH - strlen(box->label) - 3;
	len = strlen(box->value->string);
	if(len > avail) {
		kprintf(" [");
		for(i = 0; i < avail - 3; i++) {
			main_console->putch(box->value->string[i]);
		}
		kprintf("...]");
	} else {
		main_console->move_cursor(0 - len - 2, 0);
		kprintf("[%s]", box->value->string);
	}
}

/** Text box entry type. */
static ui_entry_type_t ui_textbox_entry_type = {
	.actions = ui_textbox_actions,
	.action_count = ARRAYSZ(ui_textbox_actions),
	.render = ui_textbox_render,
};

/** Create a textbox entry.
 * @param label		Label for the textbox.
 * @param value		Value to store state in (should be VALUE_TYPE_STRING).
 * @return		Pointer to created entry. */
ui_entry_t *ui_textbox_create(const char *label, value_t *value) {
	ui_textbox_t *box = kmalloc(sizeof(ui_textbox_t));

	assert(value->type == VALUE_TYPE_STRING);

	ui_entry_init(&box->header, &ui_textbox_entry_type);
	box->label = label;
	box->value = value;

	/* Create the editor window. */
	box->editor = kmalloc(sizeof(ui_window_t));
	ui_window_init(box->editor, &ui_textbox_editor_window_type, label);

	return &box->header;
}

/** Change the value a chooser.
 * @param entry		Entry to change.
 * @return		Input handling result. */
static input_result_t ui_chooser_change(ui_entry_t *entry) {
	ui_chooser_t *chooser = (ui_chooser_t *)entry;
	ui_window_display(chooser->list);
	return INPUT_RENDER;
}

/** Actions for a chooser. */
static ui_action_t ui_chooser_actions[] = {
	{ "Change", '\n', ui_chooser_change },
};

/** Render a chooser.
 * @param entry		Entry to render. */
static void ui_chooser_render(ui_entry_t *entry) {
	ui_chooser_t *chooser = (ui_chooser_t *)entry;

	kprintf("%s", chooser->label);
	main_console->move_cursor(0 - strlen(chooser->selected->name) - 2, 0);
	kprintf("[%s]", chooser->selected->name);
}

/** Chooser entry type. */
static ui_entry_type_t ui_chooser_entry_type = {
	.actions = ui_chooser_actions,
	.action_count = ARRAYSZ(ui_chooser_actions),
	.render = ui_chooser_render,
};

/** Create a chooser entry.
 * @param label		Label for the entry.
 * @param value		Value to store state in (should be VALUE_TYPE_POINTER).
 * @return		Pointer to created entry. */
ui_entry_t *ui_chooser_create(const char *label, value_t *value) {
	ui_chooser_t *chooser = kmalloc(sizeof(ui_chooser_t));

	assert(value->type == VALUE_TYPE_POINTER);

	ui_entry_init(&chooser->header, &ui_chooser_entry_type);
	chooser->label = label;
	chooser->selected = NULL;
	chooser->value = value;
	chooser->list = ui_list_create(label);
	return &chooser->header;
}

/** Select a choice.
 * @param entry		Entry to select.
 * @return		Input handling result. */
static input_result_t ui_choice_select(ui_entry_t *entry) {
	ui_choice_t *choice = (ui_choice_t *)entry;

	choice->chooser->selected = choice;
	choice->chooser->value->pointer = choice->value;
	return INPUT_CLOSE;
}

/** Actions for a choice. */
static ui_action_t ui_choice_actions[] = {
	{ "Select", '\n', ui_choice_select },
};

/** Render a choice.
 * @param entry		Entry to render. */
static void ui_choice_render(ui_entry_t *entry) {
	ui_choice_t *choice = (ui_choice_t *)entry;
	kprintf("%s", choice->name);
}

/** Chooser entry type. */
static ui_entry_type_t ui_choice_entry_type = {
	.actions = ui_choice_actions,
	.action_count = ARRAYSZ(ui_choice_actions),
	.render = ui_choice_render,
};

/** Insert a choice into a choice entry.
 * @param entry		Entry to insert into.
 * @param name		Name of the choice.
 * @param value		Value of the choice.
 * @param selected	Whether the choice should be selected. */
void ui_chooser_insert(ui_entry_t *entry, const char *name, void *value, bool selected) {
	ui_choice_t *choice = kmalloc(sizeof(ui_choice_t));
	ui_chooser_t *chooser = (ui_chooser_t *)entry;

	ui_entry_init(&choice->header, &ui_choice_entry_type);
	choice->chooser = chooser;
	choice->name = name;
	choice->value = value;

	ui_list_insert(chooser->list, &choice->header, selected);

	if(!chooser->selected || selected) {
		chooser->selected = choice;
		chooser->value->pointer = value;
	}
}
