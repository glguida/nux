/*
  EC - An embedded non standard C library
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_STRING_H
#define EC_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  size_t strlen (const char *s);
  size_t strnlen (const char *s, size_t maxlen);
  char *strchr (const char *p, int ch);
  char *strrchr (const char *p, int ch);
  size_t strcspn (const char *s, const char *charset);
  size_t strlcpy (char *dst, const char *src, size_t siz);
  char * strncpy(char *dst, const char *src, size_t n);
  int strncmp (const char *s1, const char *s2, size_t n);

  void *memset (void *b, int c, size_t len);
  void *memcpy (void *d, const void *s, size_t len);
  int memcmp (const void *s1, const void *s2, size_t len);
  void *memmove (void *d, const void *s, size_t len);
  void *memchr (const void *s, int c, size_t n);

  unsigned long fls (unsigned long);

#ifdef __cplusplus
}
#endif

#endif /* EC_STRING_H */
