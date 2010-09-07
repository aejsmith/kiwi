/*	$OpenBSD: complex.h,v 1.2 2008/12/04 03:52:31 ray Exp $	*/
/*
 * Copyright (c) 2008 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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
 * @brief		Complex arithmetic functions.
 */

#ifndef __COMPLEX_H
#define __COMPLEX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C99
 */
#ifdef __GNUC__
#if __STDC_VERSION__ < 199901
#define _Complex	__complex__
#endif
#define _Complex_I	1.0fi
#endif

#define complex		_Complex

/* XXX switch to _Imaginary_I */
#undef I
#define I		_Complex_I

/* 
 * Double versions of C99 functions
 */
extern double complex cacos(double complex);
extern double complex casin(double complex);
extern double complex catan(double complex);
extern double complex ccos(double complex);
extern double complex csin(double complex);
extern double complex ctan(double complex);
extern double complex cacosh(double complex);
extern double complex casinh(double complex);
extern double complex catanh(double complex);
extern double complex ccosh(double complex);
extern double complex csinh(double complex);
extern double complex ctanh(double complex);
extern double complex cexp(double complex);
extern double complex clog(double complex);
extern double cabs(double complex);
extern double complex cpow(double complex, double complex);
extern double complex csqrt(double complex);
extern double carg(double complex);
extern double cimag(double complex);
extern double complex conj(double complex);
extern double complex cproj(double complex);
extern double creal(double complex);

/* 
 * Float versions of C99 functions
 */
extern float complex cacosf(float complex);
extern float complex casinf(float complex);
extern float complex catanf(float complex);
extern float complex ccosf(float complex);
extern float complex csinf(float complex);
extern float complex ctanf(float complex);
extern float complex cacoshf(float complex);
extern float complex casinhf(float complex);
extern float complex catanhf(float complex);
extern float complex ccoshf(float complex);
extern float complex csinhf(float complex);
extern float complex ctanhf(float complex);
extern float complex cexpf(float complex);
extern float complex clogf(float complex);
extern float cabsf(float complex);
extern float complex cpowf(float complex, float complex);
extern float complex csqrtf(float complex);
extern float cargf(float complex);
extern float cimagf(float complex);
extern float complex conjf(float complex);
extern float complex cprojf(float complex);
extern float crealf(float complex);

/* 
 * Long double versions of C99 functions
 */
#if 0
long double complex cacosl(long double complex);
long double complex casinl(long double complex);
long double complex catanl(long double complex);
long double complex ccosl(long double complex);
long double complex csinl(long double complex);
long double complex ctanl(long double complex);
long double complex cacoshl(long double complex);
long double complex casinhl(long double complex);
long double complex catanhl(long double complex);
long double complex ccoshl(long double complex);
long double complex csinhl(long double complex);
long double complex ctanhl(long double complex);
long double complex cexpl(long double complex);
long double complex clogl(long double complex);
long double cabsl(long double complex);
long double complex cpowl(long double complex,
	long double complex);
long double complex csqrtl(long double complex);
long double cargl(long double complex);
long double cimagl(long double complex);
long double complex conjl(long double complex);
long double complex cprojl(long double complex);
long double creall(long double complex);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __COMPLEX_H */
