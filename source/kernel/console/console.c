/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		Kernel console functions.
 */

#include <io/device.h>

#include <kernel/console.h>

#include <lib/printf.h>

#include <mm/page.h>
#include <mm/vm.h>

#include <sync/spinlock.h>

#include <console.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>
#include <types.h>

/** List of kernel consoles. */
static LIST_DECLARE(console_list);
static SPINLOCK_DECLARE(console_lock);

/** Cyclic kernel log buffer. */
static struct {
	unsigned char level;		/**< Log level. */
	unsigned char ch;		/**< Character. */
} klog_buffer[CONFIG_KLOG_SIZE] __aligned(PAGE_SIZE);

/** Start of the log buffer. */
static uint32_t klog_start = 0;

/** Number of characters in the buffer. */
static uint32_t klog_length = 0;

/** Write a character to all consoles without taking the lock.
 * @param ch		Character to write. */
void console_putc_unsafe(unsigned char ch) {
	console_t *console;

	LIST_FOREACH(&console_list, iter) {
		console = list_entry(iter, console_t, header);

		if(console->putc) {
			console->putc(ch);
		}
	}
}

/** Get a character from the console without taking the lock.
 * @return		Character read. */
uint16_t console_getc_unsafe(void) {
	console_t *console;
	uint16_t ch;

	while(true) {
		LIST_FOREACH(&console_list, iter) {
			console = list_entry(iter, console_t, header);

			if(console->getc) {
				ch = console->getc();
				if(ch) {
					return ch;
				}
			}
		}
	}
}

/** Register a console.
 * @param console	Console to register. */
void console_register(console_t *console) {
	list_init(&console->header);

	spinlock_lock(&console_lock);
	list_append(&console_list, &console->header);
	spinlock_unlock(&console_lock);
}

/** Unregister a console.
 * @param console	Console to unregister. */
void console_unregister(console_t *console) {
	spinlock_lock(&console_lock);
	list_remove(&console->header);
	spinlock_unlock(&console_lock);
}

/** Handle an input character.
 * @param parser	Parser data to use.
 * @param ch		Character received.
 * @return		Value to return, 0 if no character to return yet. */
uint16_t ansi_parser_filter(ansi_parser_t *parser, unsigned char ch) {
	uint16_t ret = 0;

	if(parser->length < 0) {
		if(ch == 0x1B) {
			parser->length = 0;
			return 0;
		} else {
			return (uint16_t)ch;
		}
	} else {
		parser->buffer[parser->length++] = ch;

		/* Check for known sequences. */
		if(parser->length == 2) {
			if(strncmp(parser->buffer, "[A", 2) == 0) {
				ret = CONSOLE_KEY_UP;
			} else if(strncmp(parser->buffer, "[B", 2) == 0) {
				ret = CONSOLE_KEY_DOWN;
			} else if(strncmp(parser->buffer, "[D", 2) == 0) {
				ret = CONSOLE_KEY_LEFT;
			} else if(strncmp(parser->buffer, "[C", 2) == 0) {
				ret = CONSOLE_KEY_RIGHT;
			} else if(strncmp(parser->buffer, "[H", 2) == 0) {
				ret = CONSOLE_KEY_HOME;
			} else if(strncmp(parser->buffer, "[F", 2) == 0) {
				ret = CONSOLE_KEY_END;
			}
		} else if(parser->length == 3) {
			if(strncmp(parser->buffer, "[3~", 3) == 0) {
				ret = CONSOLE_KEY_DELETE;
			} else if(strncmp(parser->buffer, "[5~", 3) == 0) {
				ret = CONSOLE_KEY_PGUP;
			} else if(strncmp(parser->buffer, "[6~", 3) == 0) {
				ret = CONSOLE_KEY_PGDN;
			}
		}

		if(ret != 0 || parser->length == ANSI_PARSER_BUFFER_LEN) {
			parser->length = -1;
		}
		return ret;
	}
}

/** Initialise an ANSI escape code parser data structure.
 * @param parser	Parser to initialise. */
void ansi_parser_init(ansi_parser_t *parser) {
	parser->length = -1;
}

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Pointer to log level.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	int level = *(int *)data;
	console_t *console;

	/* Store in the log buffer. */
	klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].level = level;
	klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].ch = (unsigned char)ch;
	if(klog_length < CONFIG_KLOG_SIZE) {
		klog_length++;
	} else {
		klog_start = (klog_start + 1) % CONFIG_KLOG_SIZE;
	}

	/* Write to the console. */
	LIST_FOREACH(&console_list, iter) {
		console = list_entry(iter, console_t, header);

		if(console->putc && level >= console->min_level) {
			console->putc(ch);
		}
	}

	*total = *total + 1;
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param args		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kvprintf(int level, const char *fmt, va_list args) {
	int ret;

#if !CONFIG_DEBUG
	/* When debug output is disabled, do not do anything. */
	if(level == LOG_DEBUG) {
		return 0;
	}
#endif
	spinlock_lock(&console_lock);
	ret = do_printf(kvprintf_helper, &level, fmt, args);
	spinlock_unlock(&console_lock);

	return ret;
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param ...		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kprintf(int level, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = kvprintf(level, fmt, args);
	va_end(args);

	return ret;
}

/** Print out the kernel log buffer.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_log(int argc, char **argv, kdb_filter_t *filter) {
	int level = -1;
	size_t i, pos;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [/level]\n\n", argv[0]);

		kdb_printf("Prints out the contents of the kernel log buffer. If no level is specified\n");
		kdb_printf("the entire log will be printed, otherwise only characters with the specified\n");
		kdb_printf("level or higher will be printed.\n");
		kdb_printf("  Log levels:\n");
		kdb_printf("    d    Debug.\n");
		kdb_printf("    n    Normal.\n");
		kdb_printf("    w    Warning.\n");
		return KDB_SUCCESS;
	} else if(!(argc == 1 || (argc == 2 && argv[1][0] == '/'))) {
		kdb_printf("Invalid arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Look for a log level. */
	if(argc == 2) {
		argv[1]++;
		switch(*argv[1]) {
		case 'd': level = LOG_DEBUG; break;
		case 'n': level = LOG_NOTICE; break;
		case 'w': level = LOG_WARN; break;
		default:
			kdb_printf("Unknown level character '%c'\n", *argv[1]);
			return KDB_FAILURE;
		}
	}

	for(i = 0, pos = klog_start; i < klog_length; i++) {
		if(level == -1 || klog_buffer[pos].level >= level) {
			kdb_printf("%c", klog_buffer[pos].ch);
		}
		if(++pos >= CONFIG_KLOG_SIZE) {
			pos = 0;
		}
	}

	return KDB_SUCCESS;
}

/** Write to the kernel console device.
 * @param device	Device to write to.
 * @param data		Handle-specific data pointer.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Unused.
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t kconsole_device_write(device_t *device, void *data, const void *buf, size_t count,
                                      offset_t offset, size_t *bytesp) {
	const char *str = buf;
	size_t i;

	spinlock_lock(&console_lock);
	for(i = 0; i < count; i++) {
		console_putc_unsafe(str[i]);
	}
	spinlock_unlock(&console_lock);

	*bytesp = count;
	return STATUS_SUCCESS;
}

/** Handler for kernel console device requests.
 * @param device	Device request is being made on.
 * @param data		Handle specific data pointer.
 * @param request	Request number.
 * @param in		Input buffer.
 * @param insz		Input buffer size.
 * @param outp		Where to store pointer to output buffer.
 * @param outszp	Where to store output buffer size.
 * @return		Status code describing result of the operation. */
static status_t kconsole_device_request(device_t *device, void *data, int request, const void *in,
                                        size_t insz, void **outp, size_t *outszp) {
	return STATUS_NOT_IMPLEMENTED;
#if 0
	switch(request) {
	case KCONSOLE_GET_LOG_SIZE:
		return CONFIG_KLOG_SIZE;
	case KCONSOLE_READ_LOG:
		return -ERR_NOT_IMPLEMENTED;
	case KCONSOLE_CLEAR_LOG:
		spinlock_lock(&console_lock);
		klog_start = klog_length = 0;
		spinlock_unlock(&console_lock);
		return 0;
	case KCONSOLE_UPDATE_PROGRESS:
		if(insz != sizeof(int)) {
			return -ERR_PARAM_INVAL;
		}

		console_update_boot_progress(*(int *)in);
		return 0;
	default:
		return -ERR_PARAM_INVAL;
	}
#endif
}

/** Kernel console device operations structure. */
static device_ops_t kconsole_device_ops = {
	.write = kconsole_device_write,
	.request = kconsole_device_request,
};

/** Register the kernel console device. */
static __init_text void kconsole_device_init(void) {
	status_t ret;

	ret = device_create("kconsole", device_tree_root, &kconsole_device_ops,
	                    NULL, NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		fatal("Failed to register kernel console device (%d)", ret);
	}
}
INITCALL(kconsole_device_init);

/** Initialise the debug console. */
void console_early_init(void) {
	platform_console_early_init();

	/* Register the KDB command. */
	kdb_register_command("log", "Display the kernel log buffer.", kdb_cmd_log);
}
