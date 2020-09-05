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
 * @brief               Service manager main function.
 */

#include <core/log.h>
#include <core/utility.h>

#include <kernel/ipc.h>
#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>

static handle_t g_servicePort = INVALID_HANDLE;

extern const char *const *environ;

static status_t spawnProcess(const char *path) {
    process_attrib_t attrib;
    handle_t map[][2] = { { 0, 0 }, { 1, 1 }, { 2, 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = g_servicePort;
    attrib.map       = map;
    attrib.map_count = core_array_size(map);

    const char *args[] = { path, nullptr };
    status_t ret = kern_process_create(path, args, environ, 0, &attrib, nullptr);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create process '%s': %d", path, ret);
        return false;
    }

    return true;
}

int main(int argc, char **argv) {
    status_t ret;

    core_log(CORE_LOG_NOTICE, "service manager started");

    ret = kern_port_create(&g_servicePort);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create port: %d", ret);
        return EXIT_FAILURE;
    }

    spawnProcess("/system/bin/shell");

    std::vector<object_event_t> events(1);

    events[0].handle = g_servicePort;
    events[0].event  = PORT_EVENT_CONNECTION;
    events[0].flags  = 0;

    while (true) {
        ret = kern_object_wait(events.data(), events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %d", ret);
            continue;
        }
    }
}
