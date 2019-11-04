/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	GPL2.0+
*/

#ifndef _AMD64_INTERNAL
#define _AMD64_INTERNAL

#define TSS_GDTIDX(_i) (5 + (_i) * 3)

#define KCS 0x08
#define KDS 0x10
#define UCS 0x18
#define UDS 0x20

#endif
