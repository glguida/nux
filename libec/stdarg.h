/*
  EC - An embedded non standard C library
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_STDARG_H
#define EC_STDARG_H

#define va_list __builtin_va_list
#define va_start(ap,last) __builtin_va_start((ap), (last))
#define va_arg __builtin_va_arg
#define va_copy __builtin_va_copy
#define va_end __builtin_va_end

#endif /* EC_STDARG_H */
