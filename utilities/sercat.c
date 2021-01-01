/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Serial port read/write utility.
 */

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

/** Original TTY settings. */
static struct termios orig_tio;

/** Serial port FD. */
static int serial_fd;

/** At-exit handler to reset terminal. */
static void reset_term(void) {
    tcsetattr(0, TCSANOW, &orig_tio);
}

/** At-exit handler to flush port. */
static void flush_port(void) {
    tcflush(serial_fd, TCIOFLUSH);
}

/** Signal handler to reset terminal.
 * @param signo     Signal number. */
static void signal_handler(int signo) {
    /* Exit via exit() to ensure reset_term() gets called. */
    exit(0);
}

/** Initialise the TTY.
 * @return              Whether successful. */
static bool init_term(void) {
    struct termios ntio;

    if (tcgetattr(0, &orig_tio) != 0) {
        perror("tcgetattr");
        return false;
    }

    memcpy(&ntio, &orig_tio, sizeof(ntio));

    ntio.c_lflag &= ~(ECHO | ICANON);
    ntio.c_cc[VMIN] = 0;
    ntio.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSANOW, &ntio) != 0) {
        perror("tcsetattr");
        return false;
    }

    setbuf(stdout, NULL);

    /* Register an at-exit handler to reset the TTY. */
    atexit(reset_term);
    return true;
}

/** Open the serial port.
 * @param path          Path to port device.
 * @return              Whether successful. */
static bool open_serial(const char *path) {
    struct termios tio;

    serial_fd = open(path, O_RDWR);
    if (serial_fd < 0) {
        perror("open");
        return false;
    }

    if (tcgetattr(serial_fd, &tio) != 0) {
        perror("tcgetattr");
        return false;
    }

    cfsetspeed(&tio, B38400);
    tio.c_cflag |= CS8;
    tio.c_iflag &= ~ICRNL;
    tio.c_oflag = 0;
    tio.c_lflag &= ~(ECHO | ICANON);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        return false;
    }

    /* Register an at-exit handler to flush the port. */
    atexit(flush_port);
    return true;
}

int main(int argc, char **argv) {
    struct pollfd fds[] = {
        { 0, POLLIN },
        { 0, POLLIN },
    };
    char ch;

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    if (!open_serial(argv[1])) {
        return 1;
    } else if (!init_term()) {
        return 1;
    }

    fds[1].fd = serial_fd;

    /* Register signal handlers to clean up properly. */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Wait for input/output. */
    while (poll(fds, 2, -1) > 0) {
        if (fds[0].revents & POLLIN) {
            read(0, &ch, 1);
            write(serial_fd, &ch, 1);
        } else if (fds[1].revents & POLLIN) {
            read(serial_fd, &ch, 1);

            if (ch == '\r')
                continue;

            putchar(ch);
        }
    }

    return 0;
}
