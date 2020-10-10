/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Test driver.
 */

#include <module.h>
#include <status.h>

static status_t test_init(void) {
    module_t *module = module_self();
    kprintf(LOG_DEBUG, "test: loaded %p '%s'\n", module, module->name);
    return STATUS_SUCCESS;
}

static status_t test_unload(void) {
    kprintf(LOG_DEBUG, "test: unloaded\n");
    return STATUS_SUCCESS;
}

MODULE_NAME("test");
MODULE_DESC("Test driver");
MODULE_FUNCS(test_init, test_unload);
