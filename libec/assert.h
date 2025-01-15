/*
  EC - An embedded non standard C library

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef EC_ASSERT_H
#define EC_ASSERT_H

#include <cdefs.h>
#include <stdlib.h>
#include <stdio.h>

#define _str(x) #x

#define assert(_e)							\
  do {									\
    if (!(_e)) {							\
      printf("Assertion '%s' failed at "				\
	     __FILE__ ":%d\n", #_e, __LINE__);				\
      exit(-1);								\
    }									\
  } while(0)

#endif
