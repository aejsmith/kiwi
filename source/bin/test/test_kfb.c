/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel framebuffer device test.
 */

#include <core/log.h>
#include <core/time.h>
#include <core/utility.h>

#include <kernel/device/kfb.h>

#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/time.h>
#include <kernel/vm.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    status_t ret;

    handle_t handle;
    ret = kern_device_open("/virtual/kfb", FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open device: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    kfb_mode_t mode;
    ret = kern_file_request(handle, KFB_DEVICE_REQUEST_MODE, NULL, 0, &mode, sizeof(mode), NULL);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to get mode: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    core_log(CORE_LOG_NOTICE, "width:           %" PRIu16, mode.width);
    core_log(CORE_LOG_NOTICE, "height:          %" PRIu16, mode.height);
    core_log(CORE_LOG_NOTICE, "bytes_per_pixel: %" PRIu8, mode.bytes_per_pixel);
    core_log(CORE_LOG_NOTICE, "pitch:           %" PRIu32, mode.pitch);
    core_log(CORE_LOG_NOTICE, "red_position:    %" PRIu8, mode.red_position);
    core_log(CORE_LOG_NOTICE, "red_size:        %" PRIu8, mode.red_size);
    core_log(CORE_LOG_NOTICE, "green_position:  %" PRIu8, mode.green_position);
    core_log(CORE_LOG_NOTICE, "green_size:      %" PRIu8, mode.green_size);
    core_log(CORE_LOG_NOTICE, "blue_position:   %" PRIu8, mode.blue_position);
    core_log(CORE_LOG_NOTICE, "blue_size:       %" PRIu8, mode.blue_size);

    kern_thread_sleep(core_secs_to_nsecs(2), NULL);

    ret = kern_file_request(handle, KFB_DEVICE_REQUEST_ACQUIRE, NULL, 0, NULL, 0, NULL);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to acquire framebuffer: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    size_t size = core_round_up(mode.pitch * mode.height, 0x1000);

    void *mapping = NULL;
    ret = kern_vm_map(
        &mapping, size, 0, VM_ADDRESS_ANY, VM_ACCESS_READ | VM_ACCESS_WRITE,
        0, handle, 0, NULL);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to map framebuffer: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    memset(mapping, 0xff, size);

    object_event_t event = {};
    event.handle = handle;
    event.event  = KFB_DEVICE_EVENT_REDRAW;

    nstime_t now;
    kern_time_get(TIME_SYSTEM, &now);

    nstime_t target = now + core_secs_to_nsecs(10);
    while (now < target) {
        ret = kern_object_wait(&event, 1, 0, target - now);
        if (ret == STATUS_SUCCESS) {
            memset(mapping, 0xff, size);
        }

        kern_time_get(TIME_SYSTEM, &now);
    }

    return EXIT_SUCCESS;
}
