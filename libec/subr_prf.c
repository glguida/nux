/**INDENT-OFF**/
/* Imported from NetBSD -- MHDIFFIGNORE */
/*	$NetBSD: subr_prf.c,v 1.24 2014/03/22 02:51:44 hkenken Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)printf.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Scaled down version of printf(3).
 */

#if defined(_EC_SOURCE)
#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NBBY 8

typedef unsigned u_int;
typedef unsigned long u_long;
typedef long long longlong_t;
typedef unsigned long long u_longlong_t;

#define LIBSA_PRINTF_LONGLONG_SUPPORT 1
#define LIBSA_PRINTF_WIDTH_SUPPORT 1
#define LIBSA_PRINTF_PRECISION_SUPPORT 1

#else /* _EC_SOURCE */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stdint.h>		/* XXX: for intptr_t */

#if defined(_LIBC)
#include "namespace.h"
#include <stdio.h>
#include <sys/stdarg.h>
#else
#include "stand.h"
#endif

#endif /* _EC_SOURCE */

#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define INTMAX_T	longlong_t
#define UINTMAX_T	u_longlong_t
#else
#define INTMAX_T	long
#define UINTMAX_T	u_long
#endif

#if defined(_EC_SOURCE)
#define PTRDIFF_T	intptr_t
#else
#define PTRDIFF_T	ptrdiff_t
#endif

#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
static void kprintn(int (*)(int), UINTMAX_T, int, int, int);
#else
static void kprintn(int (*)(int), UINTMAX_T, int);
#endif
static int sputchar(int);
static void kdoprnt(int (*)(int), const char *, va_list);

static char *sbuf, *ebuf;

const char hexdigits[16] = "0123456789abcdef";

#define LONG		0x01
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define LLONG		0x02
#endif
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
#define ALT		0x04
#define SPACE		0x08
#define LADJUST		0x10
#define SIGN		0x20
#define ZEROPAD		0x40
#define NEGATIVE	0x80
#define KPRINTN(base)	kprintn(put, ul, base, lflag, width)
#define LZERO()							\
do {								\
	if ((lflag & (ZEROPAD|LADJUST)) == ZEROPAD) {		\
		while (width-- > 0)				\
			put('0');				\
	}							\
} while (/*CONSTCOND*/0)
#define RPAD()							\
do {								\
	if (lflag & LADJUST) {					\
		while (width-- > 0)				\
			put(' ');				\
	}							\
} while (/*CONSTCOND*/0)
#define LPAD()							\
do {								\
	if ((lflag & (ZEROPAD|LADJUST)) == 0) {			\
		while (width-- > 0)				\
			put(' ');				\
	}							\
} while (/*CONSTCOND*/0)
#else	/* LIBSA_PRINTF_WIDTH_SUPPORT */
#define KPRINTN(base)	kprintn(put, ul, base)
#define LZERO()		/**/
#define RPAD()		/**/
#define LPAD()		/**/
#endif	/* LIBSA_PRINTF_WIDTH_SUPPORT */

#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
#define KPRINT(base)						\
do {								\
	ul = (lflag & LLONG)					\
	    ? va_arg(ap, u_longlong_t)				\
	    : (lflag & LONG)					\
		? va_arg(ap, u_long)				\
		: va_arg(ap, u_int);				\
	KPRINTN(base);						\
} while (/*CONSTCOND*/0)
#else	/* LIBSA_PRINTF_LONGLONG_SUPPORT */
#define KPRINT(base)						\
do {								\
	ul = (lflag & LONG)					\
	    ? va_arg(ap, u_long) : va_arg(ap, u_int);		\
	KPRINTN(base);						\
} while (/*CONSTCOND*/0)
#endif	/* LIBSA_PRINTF_LONGLONG_SUPPORT */

static int
sputchar(int c)
{

	if (sbuf < ebuf)
		*sbuf++ = c;
	return c;
}

int
vprintf(const char * __restrict fmt, va_list ap)
{

	kdoprnt(putchar, fmt, ap);
	return 0;
}

int
vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{

	sbuf = buf;
	ebuf = buf + size - 1;
	kdoprnt(sputchar, fmt, ap);
	*sbuf = '\0';
	return sbuf - buf;
}

static void
kdoprnt(int (*put)(int), const char *fmt, va_list ap)
{
	char *p;
	int ch;
	UINTMAX_T ul;
	int lflag;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	int width;
	char *q;
#endif
#ifdef LIBSA_PRINTF_PRECISION_SUPPORT
	int prec;
#endif

	for (;;) {
		while ((ch = *fmt++) != '%') {
			if (ch == '\0')
				return;
			put(ch);
		}
		lflag = 0;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
		width = 0;
#endif
#ifdef LIBSA_PRINTF_PRECISION_SUPPORT
		prec = 0;
#endif
reswitch:
		switch (ch = *fmt++) {
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
		case '#':
			lflag |= ALT;
			goto reswitch;
		case ' ':
			lflag |= SPACE;
			goto reswitch;
		case '-':
			lflag |= LADJUST;
			goto reswitch;
		case '+':
			lflag |= SIGN;
			goto reswitch;
		case '0':
			lflag |= ZEROPAD;
			goto reswitch;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			for (;;) {
				width *= 10;
				width += ch - '0';
				ch = *fmt;
				if ((unsigned)ch - '0' > 9)
					break;
				++fmt;
			}
			goto reswitch;
#endif
#ifdef LIBSA_PRINTF_PRECISION_SUPPORT
		case '.':
			for (;;) {
				ch = *fmt;
				if ((unsigned)ch - '0' > 9)
					break;
				prec *= 10;
				prec += ch - '0';
				++fmt;
			}
			goto reswitch;
#endif
		case 'l':
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			if (*fmt == 'l') {
				++fmt;
				lflag |= LLONG;
			} else
#endif
				lflag |= LONG;
			goto reswitch;
		case 'j':
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			if (sizeof(intmax_t) == sizeof(long long))
				lflag |= LLONG;
			else
#endif
			if (sizeof(intmax_t) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 't':
			if (sizeof(PTRDIFF_T) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 'z':
			if (sizeof(ssize_t) == sizeof(long))
				lflag |= LONG;
			goto reswitch;
		case 'c':
			ch = va_arg(ap, int);
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			--width;
#endif
			LPAD();
			put(ch & 0xFF);
			RPAD();
			break;
		case 's':
			p = va_arg(ap, char *);
#ifdef _EC_SOURCE
			if (p == NULL)
			  {
			    put('{');
			    put('N');
			    put('U');
			    put('L');
			    put('L');
			    put('}');
			    break;
			  }
#endif
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			for (q = p; *q != '\0'; ++q)
				continue;
			width -= q - p;
#endif

			if (prec && ((p + prec) >= q))
				q = p + prec - 1;
			LPAD();
#ifdef LIBSA_PRINTF_PRECISION_SUPPORT
			if (prec) {
				while ((ch = (unsigned char)*p++)
				       && (--prec >= 0))
					put(ch);
			} else
			  /* PASSTHROUGH */
#endif
			while ((ch = (unsigned char)*p++))
				put(ch);
			RPAD();
			break;
		case 'd':
			ul =
#ifdef LIBSA_PRINTF_LONGLONG_SUPPORT
			(lflag & LLONG) ? va_arg(ap, longlong_t) :
#endif
			(lflag & LONG) ? va_arg(ap, long) : va_arg(ap, int);
			if ((INTMAX_T)ul < 0) {
				ul = -(INTMAX_T)ul;
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
				lflag |= NEGATIVE;
#else
				put('-');
#endif
			}
			KPRINTN(10);
			break;
		case 'o':
			KPRINT(8);
			break;
		case 'u':
			KPRINT(10);
			break;
		case 'p':
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
			lflag |= (LONG|ALT);
#else
			put('0');
			put('x');
#endif
			/* FALLTHROUGH */
		case 'X':
		case 'x':
			KPRINT(16);
			break;
		default:
			if (ch == '\0')
				return;
			put(ch);
			break;
		}
	}
}

static void
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
kprintn(int (*put)(int), UINTMAX_T ul, int base, int lflag, int width)
#else
kprintn(int (*put)(int), UINTMAX_T ul, int base)
#endif
{
					/* hold a INTMAX_T in base 8 */
	char *p, buf[(sizeof(INTMAX_T) * NBBY / 3) + 1 + 2 /* ALT + SIGN */];
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	char *q;
#endif

	p = buf;
	do {
		*p++ = hexdigits[ul % base];
	} while (ul /= base);
#ifdef LIBSA_PRINTF_WIDTH_SUPPORT
	q = p;
	if (lflag & ALT && *(p - 1) != '0') {
		if (base == 8) {
			*p++ = '0';
		} else if (base == 16) {
			*p++ = 'x';
			*p++ = '0';
		}
	}
	if (lflag & NEGATIVE)
		*p++ = '-';
	else if (lflag & SIGN)
		*p++ = '+';
	else if (lflag & SPACE)
		*p++ = ' ';
	width -= p - buf;
	if (lflag & ZEROPAD) {
		while (p > q)
			put(*--p);
	}
#endif
	LPAD();
	LZERO();
	do {
		put(*--p);
	} while (p > buf);
	RPAD();
}
