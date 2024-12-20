/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef NUX_DEFS_H
#define NUX_DEFS_H

#include <nux/hal_config.h>

#define PAGE_SHIFT HAL_PAGE_SHIFT
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SIZE - 1)

#define trunc_page(x)	((x) & (~(PAGE_SIZE - 1)))
#define round_page(x)	trunc_page((x) + PAGE_SIZE - 1)
#define ptob(x)         ((paddr_t)(x) << PAGE_SHIFT)
#define btop(x)         ((x) >> PAGE_SHIFT)

#endif
