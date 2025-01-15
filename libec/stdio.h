/*
  EC - An embedded non standard C library

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_STDIO_H
#define EC_STDIO_H

#include <cdefs.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

  int putchar (int);		/* EXTERNAL */
  int __printflike (1, 0) vprintf (const char *fmt, va_list ap);
  int __printflike (1, 0) printf (const char *fmt, ...);
  int __printflike (3, 0) vsnprintf (char *buf, size_t size,
				   const char *fmt, va_list ap);
  int __printflike (3, 0) snprintf (char *buf, size_t size,
				    const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
