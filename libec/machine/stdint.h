/*
  EC - An embedded non standard C library

  SPDX-License-Identifier:	BSD-2-Clause
*/

#if __i386__
#include <i386/stdint.h>
#elif __amd64__
#include <amd64/stdint.h>
#elif defined(__riscv) && __riscv_xlen == 64
#include <riscv64/stdint.h>
#endif
