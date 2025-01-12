/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Get string functions.
 */

#include "stdio/stdio.h"

/**
 * Read a string from standard input.
 *
 * Reads a string from standard input to a buffer. Use of this function is
 * not wise as it is not possible to tell whether a buffer-overrun occurs,
 * and therefore use of it imposes a security risk.
 *
 * @param s             Buffer to read into.
 *
 * @return              Buffer on success, NULL on failure or EOF.
 */
char *gets(char *s) {
    int i = 0, ch;

    while (true) {
        ch = fgetc(stdin);
        if (ch == EOF) {
            if (i > 0 && feof(stdin)) {
                s[i] = '\0';
                return s;
            } else {
                return NULL;
            }
        } else if (ch == '\n') {
            s[i] = '\0';
            return s;
        } else if (ch == '\b') {
            if (i)
                s[--i] = 0;
        } else {
            s[i] = ch;
        }

        i++;
    }
}

/** Read a string from a file stream.
 * @param s             Buffer to read into.
 * @param size          Maximum number of characters to read.
 * @param stream        Stream to read from.
 * @return              Buffer on success, NULL on failure or EOF. */
char *fgets(char *s, int size, FILE *stream) {
    int i, ch;

    for (i = 0; i < size - 1; i++) {
        ch = fgetc(stream);
        if (ch == EOF) {
            if (i > 0 && feof(stream)) {
                s[i] = '\0';
                return s;
            } else {
                return NULL;
            }
        } else if (ch == '\n') {
            s[i] = '\n';
            s[i + 1] = '\0';

            return s;
        } else if (ch == '\b') {
            if (i) {
                s[--i] = 0;
                i--;
            }
        } else {
            s[i] = ch;
        }
    }

    return s;
}
