/*
  EC - An embedded non standard C library

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_STDIO_H
#define EC_STDIO_H

#include <stdarg.h>

int putchar (int);		/* EXTERNAL */
int vprintf (const char *fmt, va_list ap);
int printf (const char *fmt, ...);


#endif
