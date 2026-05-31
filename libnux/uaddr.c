/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <nux/types.h>
#include <nux/hal.h>
#include <nux/nux.h>

#include "internal.h"

bool
uaddr_valid (uaddr_t a)
{
  uaddr_t min = (uaddr_t) hal_virtmem_userbase ();
  uaddr_t max = min + hal_virtmem_usersize ();

  return ((a >= min) && (a < max));
}

bool
uaddr_validrange (uaddr_t a, size_t size)
{
  uaddr_t min = (uaddr_t) hal_virtmem_userbase ();
  size_t usersize = hal_virtmem_usersize ();
  uaddr_t max;
  size_t last_offset;
  uaddr_t last;

  if (usersize > UADDR_INVALID - min)
    return false;

  max = min + usersize;

  if (size == 0)
    return ((a >= min) && (a <= max));

  if (a < min)
    return false;

  last_offset = size - 1;
  if (last_offset > UADDR_INVALID - a)
    return false;

  last = a + last_offset;
  return (last < max);
}

bool
uaddr_copyfrom (void *dst, uaddr_t src, size_t size,
		bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  return cpu_useraccess_copyfrom (dst, src, size, pf_handler);
}

bool
uaddr_copyto (uaddr_t dst, void *src, size_t size,
	       bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  return cpu_useraccess_copyto (dst, src, size, pf_handler);
}

bool
uaddr_memset (uaddr_t dst, int ch, size_t size,
	       bool (*pf_handler) (uaddr_t va, hal_pfinfo_t info))
{
  return cpu_useraccess_memset (dst, ch, size, pf_handler);
}
