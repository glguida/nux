/*
  EC - An embedded non standard C library

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_STDLIB_H
#define EC_STDLIB_H

#include <cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

  unsigned long strtoul (const char *str, char **endptr, int base);
  int atexit (void (*func) (void));
  void __dead exit (int status);	/* EXTERNAL */
  void __dead abort (void);	/* EXTERNAL */

#ifdef __cplusplus
}
#endif

#endif
