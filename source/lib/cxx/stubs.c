#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <time.h>
#include <nl_types.h>
#include <langinfo.h>
#include <pthread.h>

wint_t btowc(int c) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int catclose(nl_catd catd) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

nl_catd catopen(const char *name, int oflag) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int finite(double x) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int finitef(float x) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

void freelocale(locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int isdigit_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int isinff(float x) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int islower_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int isnanf(float ch) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int isupper_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswalpha_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswblank_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswcntrl_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswdigit_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswlower_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswprint_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswpunct_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswspace_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswupper_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int iswxdigit_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int isxdigit_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t mbrtowc(wchar_t *__restrict dst, const char *__restrict src, size_t n, mbstate_t *__restrict ps) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t mbsnrtowcs(wchar_t *__restrict dst, const char **__restrict src, size_t nms, size_t len, mbstate_t *__restrict ps) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t mbsrtowcs(wchar_t *__restrict dst, const char **__restrict src, size_t len, mbstate_t *__restrict ps) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n) {
	//fprintf(stderr, "STUB: %s\n", __func__);
	//*(int *)0xdeadc0de = 0; abort();
	return -1;
}

locale_t newlocale(int mask, const char *locale, locale_t base) {
	return (void *)0xdeadbeef;
}

int strcoll_l(const char *s1, const char *s2, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t strftime_l(char *__restrict s, size_t maxsize, const char *__restrict format, const struct tm *__restrict tm, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

float strtof(const char *__restrict s, char **__restrict endptr) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long double strtold(const char *__restrict str, char **__restrict endptr) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long double strtold_l(const char *__restrict str, char **__restrict endp, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long long strtoll_l(const char *__restrict str, char **__restrict endp, int base, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

unsigned long long strtoull_l(const char *__restrict str, char **__restrict endp, int base, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t strxfrm_l(char *__restrict s1, const char *__restrict s2, size_t n, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int swprintf(wchar_t *__restrict ws, size_t n, const wchar_t *__restrict format, ...) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int tolower_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int toupper_l(int ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wint_t towlower_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wint_t towupper_l(wint_t ch, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

locale_t uselocale(locale_t locale) {
	//fprintf(stderr, "STUB: %s\n", __func__);
	//*(int *)0xdeadc0de = 0; abort();
	return locale;
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict ps) {
	//fprintf(stderr, "STUB: %s\n", __func__);
	//*(int *)0xdeadc0de = 0; abort();
	return -1;
}

int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t wcslen(const wchar_t *ws) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t wcsnrtombs(char *__restrict dst, const wchar_t **__restrict src, size_t nwc, size_t len, mbstate_t *__restrict ps) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

double wcstod(const wchar_t *__restrict s, wchar_t **__restrict endptr) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

float wcstof(const wchar_t *__restrict s, wchar_t **__restrict endptr) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long wcstol(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long double wcstold(const wchar_t *__restrict s, wchar_t **__restrict endptr) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

long long wcstoll(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

unsigned long wcstoul(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

unsigned long long wcstoull(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

size_t wcsxfrm_l(wchar_t *__restrict ws1, const wchar_t *__restrict ws2, size_t n, locale_t locale) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int wctob(wint_t c) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wchar_t *wmemcpy(wchar_t *__restrict dst, const wchar_t *__restrict src, size_t n) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wchar_t *wmemset(wchar_t *s, wchar_t v, size_t n) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

wchar_t *wmemmove(wchar_t *dst, const wchar_t *src, size_t n) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

char *catgets(nl_catd catd, int set_id, int msg_id, const char *s) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int pthread_create(pthread_t *__restrict thread, const pthread_attr_t *__restrict attr,
	void *(*func)(void *), void *__restrict arg)
{
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int pthread_detach(pthread_t thread) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}

int pthread_join(pthread_t thread, void **val) {
	fprintf(stderr, "STUB: %s\n", __func__);
	*(int *)0xdeadc0de = 0; abort();
}
