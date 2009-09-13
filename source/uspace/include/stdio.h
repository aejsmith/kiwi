/* Standard I/O functions
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Standard I/O functions.
 */

#ifndef __STDIO_H
#define __STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#define __need_NULL
#include <stddef.h>
#include <stdarg.h>

struct __fstream_internal;

/** Type describing an open file stream. */
typedef struct __fstream_internal FILE;

/** End of file return value. */
#define EOF		(-1)

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#if 0
/** Type describing a file offset. */
typedef offset_t fpos_t;

/** Size of buffers for IO streams. */
#define BUFSIZ		2048

/** Buffer flags for an IO stream. */
#define _IOFBF		0		/**< Input/output fully buffered. */
#define _IOLBF		1		/**< Input/output line buffered. */
#define _IONBF		2		/**< Input/output unbuffered. */

/** Minimum number of unique files from tmpnam() and friends. */
#define TMP_MAX		10000

/** Maximum number of streams that can be open simultaneously. */
#define FOPEN_MAX	32

/** Maximum length of a filename string. */
#define FILENAME_MAX	4096

/** Directory usable for creating temporary files. */
//#define P_tmpdir	"/System/Temp"
#endif

/** Actions for fseek(). */
#define SEEK_SET	1	/**< Set the offset to the exact position specified. */
#define SEEK_CUR	2	/**< Add the supplied value to the current offset. */
#define SEEK_END	3	/**< Set the offset to the end of the file plus the supplied value. */

extern void clearerr(FILE *stream);
/* char *ctermid(char *); */
extern int fclose(FILE *stream);
extern int feof(FILE *stream);
extern int ferror(FILE *stream);
extern int fflush(FILE *stream);
extern int fgetc(FILE *stream);
/* int fgetpos(FILE *, fpos_t *); */
extern char *fgets(char *s, int size, FILE *stream);
//extern int fileno(FILE *stream);
/* void flockfile(FILE *); */
extern FILE *fopen(const char *path, const char *mode);
extern int fprintf(FILE *stream, const char *fmt, ...);
extern int fputc(int ch, FILE *stream);
extern int fputs(const char *s, FILE *stream);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern FILE *freopen(const char *path, const char *mode, FILE *stream);
//extern int fscanf(FILE *stream, const char *fmt, ...);
extern int fseek(FILE *stream, long off, int act);
/* int fsetpos(FILE *, const fpos_t *); */
extern long ftell(FILE *stream);
/* int ftrylockfile(FILE *); */
/* void funlockfile(FILE *); */
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int getc(FILE *stream);
extern int getchar(void);
/* int getc_unlocked(FILE *); */
/* int getchar_unlocked(void); */
extern char *gets(char *s);
/* int      pclose(FILE *); */
//extern void perror(const char *s);
/* FILE    *popen(const char *, const char *); */
extern int printf(const char *fmt, ...);
extern int putc(int ch, FILE *stream);
extern int putchar(int ch);
/* int putc_unlocked(int, FILE *); */
/* int putchar_unlocked(int); */
extern int puts(const char *s);
//extern int remove(const char *path);
//extern int rename(const char *source, const char *dest);
extern void rewind(FILE *stream);
//extern int scanf(const char *fmt, ...);
/* void setbuf(FILE *, char *); */
/* int setvbuf(FILE *, char *, int, size_t); */
extern int snprintf(char *buf, size_t size, const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);
//extern int sscanf(const char *buf, const char *fmt, ...);
/* char *tempnam(const char *, const char *); */
/* FILE *tmpfile(void); */
/* char *tmpnam(char *); */
//extern int ungetc(int ch, FILE *stream);
extern int vfprintf(FILE *stream, const char *fmt, va_list args);
//extern int vfscanf(FILE *stream, const char *fmt, va_list args);
extern int vprintf(const char *fmt, va_list args);
//extern int vscanf(const char *fmt, va_list args);
extern int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
extern int vsprintf(char *buf, const char *fmt, va_list args);
//extern int vsscanf(const char *buf, const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* __STDIO_H */
