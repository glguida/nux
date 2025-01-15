/*
  EC - An embedded non standard C library
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_LIMITS_H
#define EC_LIMITS_H

#include <machine/limits.h>

#define SCHAR_MIN (-__SCHAR_MAX__-1)
#define SCHAR_MAX __SCHAR_MAX__
#define UCHAR_MAX (2*SCHAR_MAX+1)

#define SHRT_MIN  (-__SHRT_MAX__-1)
#define SHRT_MAX  __SHRT_MAX__
#define USHRT_MAX (2*SHRT_MAX+1)

#define INT_MIN (-__INT_MAX__-1)
#define INT_MAX __INT_MAX__
#define UINT_MAX (2U*INT_MAX+1U)

#define LONG_MIN (-__LONG_MAX__-1L)
#define LONG_MAX __LONG_MAX__
#define ULONG_MAX (2UL*LONG_MAX+1UL)

#define LLONG_MIN (-__LONG_LONG_MAX__-1LL)
#define LLONG_MAX __LONG_LONG_MAX__
#define ULLONG_MAX (2ULL*LLONG_MAX+1ULL)

#define INT32_MAX 0x7fffffff
#define UINT32_MAX 0xffffffff

#define INT64_MAX 0x7fffffffffffffffLL
#define UINT64_MAX 0xffffffffffffffffLL

#endif
